// test_cache.cpp - Cache semantics, edge cases, property checks, OPT, AI, Adaptive.
#include <random>

#include "AIPolicy.h"
#include "AdaptivePolicy.h"
#include "BeladyPolicy.h"
#include "Cache.h"
#include "LRUPolicy.h"
#include "TestUtil.h"
#include "Workloads.h"
#include "doctest.h"

using namespace cachesim;

namespace {
std::vector<Key> randomTrace(std::size_t n, std::size_t universe, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<std::size_t> d(0, universe - 1);
    std::vector<Key> t;
    for (std::size_t i = 0; i < n; ++i) t.push_back(static_cast<Key>(d(rng)));
    return t;
}
const std::vector<std::string> kOnline{"fifo", "lru", "lfu", "ai", "ai-sampled", "adaptive"};
}  // namespace

TEST_CASE("Cache: capacity 0 and null policy are rejected") {
    CHECK_THROWS_AS(Cache(0, std::make_unique<LRUPolicy>()), std::invalid_argument);
    CHECK_THROWS_AS(Cache(3, nullptr), std::invalid_argument);
    CHECK_THROWS_AS(Simulator(0), std::invalid_argument);
}

TEST_CASE("Cache: get/put semantics") {
    Cache c(2, std::make_unique<LRUPolicy>());
    CHECK_FALSE(c.get(1).has_value());
    c.put(1, 100);
    c.put(2, 200);
    REQUIRE(c.get(1).has_value());
    CHECK(*c.get(1) == 100);
    c.put(1, 111);                   // update in place, also refreshes recency
    c.put(3, 300);                   // evicts 2 (LRU)
    CHECK(c.contains(1));
    CHECK_FALSE(c.contains(2));
    CHECK(*c.get(1) == 111);
    CHECK(c.size() == 2);
    CHECK(c.keys() == std::vector<Key>{1, 3});
}

TEST_CASE("Cache: runtime policy swap keeps residents") {
    Cache c(3, std::make_unique<LRUPolicy>());
    for (Key k : {1, 2, 3}) c.access(k);
    c.setPolicy(PolicyFactory::create("fifo"));
    CHECK(c.policy().name() == "FIFO");
    CHECK(c.policy().trackedCount() == 3);
    c.access(4);
    CHECK(c.size() == 3);
}

TEST_CASE("Edge: empty workload") {
    for (const auto& p : kOnline) {
        const auto r = testutil::run(p, {}, 3);
        CHECK(r.hits + r.misses == 0);
    }
}

TEST_CASE("Edge: capacity >= unique keys -> zero evictions") {
    const auto t = randomTrace(5000, 40, 1);
    for (const auto& p : kOnline) {
        const auto r = testutil::run(p, t, 40);
        CHECK(r.evictions == 0);
        CHECK(r.misses == 40);  // only compulsory misses
    }
}

TEST_CASE("Property: size <= capacity, policy in sync, hits + misses == requests") {
    const auto t = randomTrace(20000, 300, 7);
    for (std::size_t cap : {1u, 2u, 17u, 64u}) {
        for (const auto& name : PolicyFactory::available()) {
            PolicyConfig cfg;
            cfg.capacity = cap;
            cfg.futureTrace = &t;
            testutil::CapacityGuard guard;
            const auto m = Simulator(cap).run(t, PolicyFactory::create(name, cfg), &guard);
            CAPTURE(name);
            CAPTURE(cap);
            CHECK_FALSE(guard.violated);
            CHECK_FALSE(guard.desynced);
            CHECK(m.hits + m.misses == t.size());
        }
    }
}

TEST_CASE("Belady: hand trace 1 2 3 1 4 2 5, capacity 3") {
    // At '4': next uses 1=never, 2=pos 5, 3=never -> evicts a never-used key (3).
    const auto r = testutil::run("belady", testutil::sample(), 3);
    CHECK(r.hits == 2);
    CHECK(r.misses == 5);
    CHECK(r.victims == std::vector<Key>{3, 4});
}

TEST_CASE("Belady: hit ratio >= every online policy on many traces") {
    WorkloadParams wp;
    wp.capacity = 20;
    wp.universe = 300;
    for (const auto& w : WorkloadFactory::available()) {
        for (std::uint64_t seed : {1u, 2u, 3u}) {
            wp.seed = seed;
            const auto t = WorkloadFactory::create(w, wp)->generate(5000);
            const auto opt = testutil::run("belady", t, 20);
            for (const auto& p : kOnline) {
                CAPTURE(w);
                CAPTURE(p);
                CHECK(opt.hits >= testutil::run(p, t, 20).hits);
            }
        }
    }
}

TEST_CASE("Belady: rejects a request stream that differs from its trace") {
    BeladyOptimalPolicy p({1, 2, 3});
    p.onInsert(1);
    CHECK_THROWS_AS(p.onInsert(3), std::logic_error);
}

TEST_CASE("AI: weight vectors reproduce classic policies exactly") {
    const auto t = randomTrace(20000, 200, 11);
    PolicyConfig lruLike;
    lruLike.aiWeights = AIWeights({1.0, 0.0, 0.0, 0.0});
    PolicyConfig fifoLike;
    fifoLike.aiWeights = AIWeights({0.0, 0.0, 1.0, 0.0});
    CHECK(testutil::run("ai", t, 32, lruLike).victims == testutil::run("lru", t, 32).victims);
    CHECK(testutil::run("ai", t, 32, fifoLike).victims == testutil::run("fifo", t, 32).victims);
}

TEST_CASE("AI: features are normalised to [0,1]") {
    AIPolicy p;
    for (Key k : {1, 2, 3}) p.onInsert(k);
    p.onAccess(1);
    p.onAccess(1);
    for (Key k : {1, 2, 3}) {
        for (double f : p.featuresOf(k)) {
            CHECK(f >= 0.0);
            CHECK(f <= 1.0);
        }
    }
    CHECK(p.featuresOf(1)[1] == doctest::Approx(1.0));  // most frequent key
    CHECK(p.featuresOf(1)[0] < p.featuresOf(2)[0]);      // 1 is more recent than 2
}

TEST_CASE("AI: sampled mode with K >= size equals full scan") {
    const auto t = randomTrace(5000, 100, 3);
    PolicyConfig cfg;
    cfg.sampleK = 64;  // bigger than capacity 16
    CHECK(testutil::run("ai-sampled", t, 16, cfg).victims == testutil::run("ai", t, 16).victims);
}

TEST_CASE("AI: wrong number of weights is rejected") {
    CHECK_THROWS_AS(AIWeights({1.0, 2.0}), std::invalid_argument);
}

TEST_CASE("Adaptive: follows the best expert") {
    // Scan-heavy trace: LFU's shadow should win, so Adaptive ends up near LFU.
    ScanResistanceWorkload scan(40, 500, 100, 5);
    const auto t = scan.generate(30000);
    const auto ad = testutil::run("adaptive", t, 50);
    const auto lru = testutil::run("lru", t, 50);
    CHECK(ad.hits > lru.hits);
    AdaptivePolicy p(10, 100);
    CHECK(p.currentLeader() == "LRU");
}

TEST_CASE("Observer: removed listeners stop receiving events") {
    Cache c(2, std::make_unique<LRUPolicy>());
    testutil::VictimLog log;
    c.addListener(&log);
    for (Key k : {1, 2, 3}) c.access(k);
    c.removeListener(&log);
    c.access(4);
    CHECK(log.victims == std::vector<Key>{1});
}
