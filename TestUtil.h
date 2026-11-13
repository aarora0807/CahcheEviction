// TestUtil.h - helpers shared by the test files.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Cache.h"
#include "PolicyFactory.h"
#include "Simulator.h"

namespace testutil {

using cachesim::Key;

/// Records every victim in order.
class VictimLog final : public cachesim::CacheListener {
public:
    void onEvict(Key victim, std::int64_t) override { victims.push_back(victim); }
    std::vector<Key> victims;
};

/// Checks the size invariant after every single request.
class CapacityGuard final : public cachesim::CacheListener {
public:
    void onRequestComplete(const cachesim::Cache& c, Key) override {
        if (c.size() > c.capacity()) violated = true;
        if (c.policy().trackedCount() != c.size()) desynced = true;
    }
    bool violated = false;
    bool desynced = false;
};

struct Outcome {
    std::uint64_t hits = 0, misses = 0, evictions = 0;
    std::vector<Key> victims;
};

inline Outcome run(const std::string& policy, const std::vector<Key>& trace, std::size_t capacity,
                   cachesim::PolicyConfig cfg = cachesim::PolicyConfig{}) {
    cfg.capacity = capacity;
    cfg.futureTrace = &trace;
    VictimLog log;
    const cachesim::Simulator sim(capacity);
    const auto m = sim.run(trace, cachesim::PolicyFactory::create(policy, cfg), &log);
    return Outcome{m.hits, m.misses, m.evictions, log.victims};
}

inline std::vector<Key> sample() { return {1, 2, 3, 1, 4, 2, 5}; }

}  // namespace testutil
