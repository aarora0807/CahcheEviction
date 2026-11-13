// AdaptivePolicy.cpp
#include "AdaptivePolicy.h"

#include <stdexcept>

#include "FIFOPolicy.h"
#include "LFUPolicy.h"
#include "LRUPolicy.h"

namespace cachesim {

// -------------------------------------------------------------- ShadowCache
ShadowCache::ShadowCache(std::size_t capacity, std::unique_ptr<EvictionPolicy> policy)
    : capacity_(capacity), policy_(std::move(policy)) {
    if (capacity_ == 0) throw std::invalid_argument("ShadowCache capacity must be > 0");
}

bool ShadowCache::request(Key key) {
    if (keys_.count(key) != 0) {
        policy_->onAccess(key);
        return true;
    }
    if (keys_.size() >= capacity_) {
        const Key victim = policy_->selectVictim();
        keys_.erase(victim);
        policy_->onRemove(victim);
    }
    keys_.insert(key);
    policy_->onInsert(key);
    return false;
}

// ----------------------------------------------------------- AdaptivePolicy
AdaptivePolicy::AdaptivePolicy(std::size_t capacity, std::size_t window)
    : window_(window == 0 ? 1 : window) {
    if (capacity == 0) throw std::invalid_argument("AdaptivePolicy capacity must be > 0");
    experts_.push_back(Expert{std::make_unique<FIFOPolicy>(),
                              ShadowCache(capacity, std::make_unique<FIFOPolicy>()), {}, 0});
    experts_.push_back(Expert{std::make_unique<LRUPolicy>(),
                              ShadowCache(capacity, std::make_unique<LRUPolicy>()), {}, 0});
    experts_.push_back(Expert{std::make_unique<LFUPolicy>(),
                              ShadowCache(capacity, std::make_unique<LFUPolicy>()), {}, 0});
}

void AdaptivePolicy::observe(Key key) {
    for (Expert& e : experts_) {
        const bool hit = e.shadow.request(key);
        e.recent.push_back(hit);
        if (hit) ++e.windowHits;
        if (e.recent.size() > window_) {  // slide the window
            if (e.recent.front()) --e.windowHits;
            e.recent.pop_front();
        }
    }
    // Switch only when another expert is strictly better (hysteresis).
    std::size_t best = leader_;
    for (std::size_t i = 0; i < experts_.size(); ++i) {
        if (experts_[i].windowHits > experts_[best].windowHits) best = i;
    }
    if (best != leader_) {
        leader_ = best;
        ++switches_;
    }
}

void AdaptivePolicy::onInsert(Key key) {
    for (Expert& e : experts_) e.real->onInsert(key);
    observe(key);
}

void AdaptivePolicy::onAccess(Key key) {
    for (Expert& e : experts_) e.real->onAccess(key);
    observe(key);
}

void AdaptivePolicy::onRemove(Key key) {
    for (Expert& e : experts_) e.real->onRemove(key);
}

Key AdaptivePolicy::selectVictim() { return experts_[leader_].real->selectVictim(); }

std::size_t AdaptivePolicy::trackedCount() const { return experts_[leader_].real->trackedCount(); }

std::string AdaptivePolicy::currentLeader() const { return experts_[leader_].real->name(); }

}  // namespace cachesim
