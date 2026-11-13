// test_lru.cpp - LRU hand-traced behaviour.
#include "LRUPolicy.h"
#include "TestUtil.h"
#include "doctest.h"

using namespace cachesim;

TEST_CASE("LRU: hand trace 1 2 3 1 4 2 5, capacity 3") {
    // 1M 2M 3M 1H [1,3,2]  4M evict 2 [4,1,3]  2M evict 3 [2,4,1]  5M evict 1
    const auto r = testutil::run("lru", testutil::sample(), 3);
    CHECK(r.hits == 1);
    CHECK(r.misses == 6);
    CHECK(r.victims == std::vector<Key>{2, 3, 1});
}

TEST_CASE("LRU: access moves a key to MRU") {
    LRUPolicy p;
    p.onInsert(1);
    p.onInsert(2);
    p.onInsert(3);
    CHECK(p.selectVictim() == 1);
    p.onAccess(1);
    CHECK(p.selectVictim() == 2);
}

TEST_CASE("LRU: loop one larger than the cache never hits (worst case)") {
    std::vector<Key> loop;
    for (int rep = 0; rep < 10; ++rep)
        for (Key k = 0; k < 4; ++k) loop.push_back(k);
    const auto r = testutil::run("lru", loop, 3);
    CHECK(r.hits == 0);
}

TEST_CASE("LRU: single key repeated -> one compulsory miss") {
    const auto r = testutil::run("lru", std::vector<Key>(100, 5), 2);
    CHECK(r.misses == 1);
    CHECK(r.hits == 99);
    CHECK(r.evictions == 0);
}

TEST_CASE("LRU: empty policy cannot choose a victim") {
    LRUPolicy p;
    CHECK_THROWS_AS(p.selectVictim(), std::logic_error);
}
