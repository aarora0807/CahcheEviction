// Workload.cpp - the Template Method skeleton.
#include "Workload.h"

namespace cachesim {

std::vector<Key> WorkloadGenerator::generate(std::size_t n) {
    rng_.seed(seed_);  // identical trace on every call -> reproducible experiments
    const std::size_t length = resolveLength(n);
    prepare(length);
    std::vector<Key> trace;
    trace.reserve(length);
    for (std::size_t i = 0; i < length; ++i) trace.push_back(nextKey(i, length));
    return trace;
}

}  // namespace cachesim
