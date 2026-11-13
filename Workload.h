// Workload.h - abstract request-stream generator (Template Method).
#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "Types.h"

namespace cachesim {

/// [Design pattern: TEMPLATE METHOD]
/// generate() fixes the algorithm skeleton (reseed -> prepare -> loop nextKey);
/// subclasses only override the key-selection step. Reseeding inside
/// generate() makes every call return the identical, reproducible trace.
class WorkloadGenerator {
public:
    explicit WorkloadGenerator(std::uint64_t seed) : seed_(seed), rng_(seed) {}
    virtual ~WorkloadGenerator() = default;
    WorkloadGenerator(const WorkloadGenerator&) = delete;
    WorkloadGenerator& operator=(const WorkloadGenerator&) = delete;
    WorkloadGenerator(WorkloadGenerator&&) = delete;
    WorkloadGenerator& operator=(WorkloadGenerator&&) = delete;

    /// Produce n requests (n == 0 means "natural length", used by trace files).
    std::vector<Key> generate(std::size_t n);
    virtual std::string name() const = 0;

protected:
    /// Hook: reset per-run state before the first nextKey().
    virtual void prepare(std::size_t /*n*/) {}
    /// Hook: actual length for a requested n (trace files override for n == 0).
    virtual std::size_t resolveLength(std::size_t n) const { return n; }
    /// The primitive operation: the key of request i out of n.
    virtual Key nextKey(std::size_t i, std::size_t n) = 0;
    std::mt19937_64& rng() { return rng_; }

private:
    std::uint64_t seed_;
    std::mt19937_64 rng_;
};

}  // namespace cachesim
