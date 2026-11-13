// AdaptivePolicy.h - picks FIFO/LRU/LFU online using shadow (ghost) caches.
#pragma once

#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "EvictionPolicy.h"

namespace cachesim {

/// A keys-only simulation of a cache: no values, just "would this have hit?".
class ShadowCache {
public:
    ShadowCache(std::size_t capacity, std::unique_ptr<EvictionPolicy> policy);
    /// Feed one request; returns true if the shadow cache would have hit.
    bool request(Key key);

private:
    std::size_t capacity_;
    std::unique_ptr<EvictionPolicy> policy_;
    std::unordered_set<Key> keys_;
};

/// Inspired by ARC / LeCaR: run several "experts" side by side.
///  * Each expert has a SHADOW cache that sees every request and records,
///    over a sliding window of W requests, how often it would have hit.
///  * Each expert also has a REAL policy instance that tracks the actual
///    resident set, so it can name a valid victim at any time.
/// On eviction we delegate to the real instance of the current leader
/// (highest windowed hit count). Cost: 3x bookkeeping, still O(1) per expert.
class AdaptivePolicy final : public EvictionPolicy {
public:
    explicit AdaptivePolicy(std::size_t capacity, std::size_t window = 1000);

    void onInsert(Key key) override;
    void onAccess(Key key) override;
    void onRemove(Key key) override;
    Key selectVictim() override;
    std::string name() const override { return "Adaptive"; }
    std::size_t trackedCount() const override;

    std::string currentLeader() const;
    std::size_t switches() const { return switches_; }

private:
    struct Expert {
        std::unique_ptr<EvictionPolicy> real;
        ShadowCache shadow;
        std::deque<bool> recent;  // last W shadow outcomes
        std::size_t windowHits = 0;
    };
    void observe(Key key);  // update shadows + leader for one request

    std::size_t window_;
    std::vector<Expert> experts_;
    std::size_t leader_ = 1;  // start with LRU
    std::size_t switches_ = 0;
};

}  // namespace cachesim
