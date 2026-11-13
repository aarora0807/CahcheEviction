// PolicyFactory.cpp
#include "PolicyFactory.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "AdaptivePolicy.h"
#include "BeladyPolicy.h"
#include "FIFOPolicy.h"
#include "LFUPolicy.h"
#include "LRUPolicy.h"

namespace cachesim {

std::unique_ptr<EvictionPolicy> PolicyFactory::create(const std::string& rawName,
                                                      const PolicyConfig& cfg) {
    std::string name = rawName;
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (name == "fifo") return std::make_unique<FIFOPolicy>();
    if (name == "lru") return std::make_unique<LRUPolicy>();
    if (name == "lfu") return std::make_unique<LFUPolicy>();
    if (name == "ai") return std::make_unique<AIPolicy>(cfg.aiWeights, 0, cfg.seed);
    if (name == "ai-sampled") {
        return std::make_unique<AIPolicy>(cfg.aiWeights, cfg.sampleK == 0 ? 8 : cfg.sampleK,
                                          cfg.seed);
    }
    if (name == "adaptive") {
        if (cfg.capacity == 0) throw std::invalid_argument("adaptive policy needs capacity > 0");
        return std::make_unique<AdaptivePolicy>(cfg.capacity, cfg.adaptiveWindow);
    }
    if (name == "belady" || name == "opt") {
        if (cfg.futureTrace == nullptr) {
            throw std::invalid_argument("belady policy needs the future trace (offline only)");
        }
        return std::make_unique<BeladyOptimalPolicy>(*cfg.futureTrace);
    }
    throw std::invalid_argument("unknown policy '" + rawName +
                                "' (try: fifo, lru, lfu, ai, ai-sampled, adaptive, belady)");
}

std::vector<std::string> PolicyFactory::available() {
    return {"fifo", "lru", "lfu", "ai", "ai-sampled", "adaptive", "belady"};
}

}  // namespace cachesim
