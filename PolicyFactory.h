// PolicyFactory.h - builds any policy from its name.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "AIPolicy.h"
#include "EvictionPolicy.h"

namespace cachesim {

/// Extra inputs some policies need. Fields a policy doesn't use are ignored.
struct PolicyConfig {
    std::size_t capacity = 0;                       // "adaptive" (shadow caches)
    const std::vector<Key>* futureTrace = nullptr;  // "belady" (non-owning)
    AIWeights aiWeights = AIWeights::defaults();    // "ai", "ai-sampled"
    std::size_t sampleK = 8;                        // "ai-sampled"
    std::size_t adaptiveWindow = 1000;              // "adaptive"
    std::uint64_t seed = 7;                         // sampling RNG
};

/// [Design pattern: FACTORY]
/// Callers depend only on the EvictionPolicy interface; adding a new policy
/// means one new class + one line here (Open/Closed principle).
class PolicyFactory {
public:
    /// Names: fifo, lru, lfu, ai, ai-sampled, adaptive, belady (alias opt).
    /// Throws std::invalid_argument for unknown names / missing config.
    static std::unique_ptr<EvictionPolicy> create(const std::string& name,
                                                  const PolicyConfig& config = PolicyConfig{});
    static std::vector<std::string> available();
};

}  // namespace cachesim
