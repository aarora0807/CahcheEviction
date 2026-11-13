// test_fifo.cpp - FIFO hand-traced behaviour.
#include "FIFOPolicy.h"
#include "TestUtil.h"
#include "doctest.h"

using namespace cachesim;

TEST_CASE("FIFO: hand trace 1 2 3 1 4 2 5, capacity 3") {
    // 1M 2M 3M 1H 4M(evict 1) 2H 5M(evict 2)
    const auto r = testutil::run("fifo", testutil::sample(), 3);
    CHECK(r.hits == 2);
    CHECK(r.misses == 5);
    CHECK(r.victims == std::vector<Key>{1, 2});
}

TEST_CASE("FIFO: a hit does not refresh insertion order") {
    FIFOPolicy p;
    p.onInsert(10);
    p.onInsert(20);
    p.onAccess(10);  // would save 10 under LRU, not under FIFO
    CHECK(p.selectVictim() == 10);
}

TEST_CASE("FIFO: capacity 1 evicts on every new key") {
    const auto r = testutil::run("fifo", {7, 7, 8, 7}, 1);
    CHECK(r.hits == 1);
    CHECK(r.victims == std::vector<Key>{7, 8});
}

TEST_CASE("FIFO: remove keeps the queue consistent") {
    FIFOPolicy p;
    p.onInsert(1);
    p.onInsert(2);
    p.onInsert(3);
    p.onRemove(1);
    CHECK(p.trackedCount() == 2);
    CHECK(p.selectVictim() == 2);
    p.onRemove(42);  // unknown key is ignored
    CHECK(p.trackedCount() == 2);
}

TEST_CASE("FIFO: empty policy cannot choose a victim") {
    FIFOPolicy p;
    CHECK_THROWS_AS(p.selectVictim(), std::logic_error);
}
