// Simulator.h - runs one (trace, policy, capacity) experiment.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Cache.h"
#include "Metrics.h"

namespace cachesim {

/// One row of a comparison table.
struct SimulationResult {
    std::string workload;
    std::string policy;
    std::size_t capacity = 0;
    Metrics metrics;
};

/// Stateless apart from its capacity, so one Simulator can be reused and
/// (being const) safely shared by many threads.
class Simulator {
public:
    explicit Simulator(std::size_t capacity);  // throws std::invalid_argument on 0

    /// Replays the trace through a fresh Cache. `observer` (optional, non-owning)
    /// receives every cache event, e.g. a Tracer.
    Metrics run(const std::vector<Key>& trace, std::unique_ptr<EvictionPolicy> policy,
                CacheListener* observer = nullptr, bool measureDecisions = true) const;

    std::size_t capacity() const { return capacity_; }

private:
    std::size_t capacity_;
};

}  // namespace cachesim
