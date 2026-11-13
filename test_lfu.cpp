// test_lfu.cpp - LFU hand-traced behaviour.
#include "LFUPolicy.h"
#include "TestUtil.h"
#include "doctest.h"

using namespace cachesim;

TEST_CASE("LFU: hand trace 1 2 3 1 4 2 5, capacity 3") {
    // f(1)=2 after the hit; ties among f=1 broken by LRU:
    // 4M evict 2, 2M evict 3, 5M evict 4
    const auto r = testutil::run("lfu", testutil::sample(), 3);
    CHECK(r.hits == 1);
    CHECK(r.misses == 6);
    CHECK(r.victims == std::vector<Key>{2, 3, 4});
}

TEST_CASE("LFU: frequencies and min-frequency tracking") {
    LFUPolicy p;
    p.onInsert(1);
    p.onInsert(2);
    p.onAccess(1);
    p.onAccess(1);
    CHECK(p.frequencyOf(1) == 3);
    CHECK(p.frequencyOf(2) == 1);
    CHECK(p.selectVictim() == 2);
    p.onRemove(2);  // min bucket disappears -> minFreq must move to 3
    CHECK(p.selectVictim() == 1);
    p.onInsert(9);  // new key -> minFreq back to 1
    CHECK(p.selectVictim() == 9);
}

TEST_CASE("LFU: LRU tie-break inside a frequency bucket") {
    LFUPolicy p;
    p.onInsert(1);
    p.onInsert(2);
    p.onInsert(3);
    p.onAccess(1);
    p.onAccess(3);  // f: 1->2, 2->1, 3->2
    CHECK(p.selectVictim() == 2);
    p.onRemove(2);
    CHECK(p.selectVictim() == 1);  // 1 and 3 tie at f=2, 1 is less recent
}

TEST_CASE("LFU: protects a hot key against a one-time scan") {
    std::vector<Key> t;
    for (int i = 0; i < 20; ++i) t.push_back(0);   // hot key
    for (Key k = 100; k < 140; ++k) t.push_back(k);  // scan
    t.push_back(0);
    const auto lfu = testutil::run("lfu", t, 4);
    const auto lru = testutil::run("lru", t, 4);
    CHECK(lfu.hits == lru.hits + 1);  // only LFU still has key 0 at the end
}

TEST_CASE("LFU: empty policy cannot choose a victim") {
    LFUPolicy p;
    CHECK_THROWS_AS(p.selectVictim(), std::logic_error);
}
