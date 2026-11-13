// LFUPolicy.h - Least Frequently Used replacement with LRU tie-breaking.
#pragma once

#include <list>
#include <map>
#include <unordered_map>

#include "EvictionPolicy.h"

namespace cachesim {

/// Classic O(1) LFU ("frequency buckets"):
///   nodes_   : key -> {frequency, iterator into its bucket list}
///   buckets_ : frequency -> list of keys (front = most recent in bucket)
///   minFreq_ : smallest frequency present, so the victim is found in O(1).
/// Ties inside a bucket are broken by LRU (victim = bucket back).
/// std::map is used for buckets so the minimum is also recoverable via
/// begin() after an arbitrary onRemove(); the number of distinct frequencies
/// F is tiny, so the O(log F) map insert is effectively constant.
class LFUPolicy final : public EvictionPolicy {
public:
    void onInsert(Key key) override;
    void onAccess(Key key) override;
    void onRemove(Key key) override;
    Key selectVictim() override;
    std::string name() const override { return "LFU"; }
    std::size_t trackedCount() const override { return nodes_.size(); }

    /// Access count of a tracked key (0 if not tracked). Used by tests.
    int frequencyOf(Key key) const;

private:
    struct FreqNode {
        int freq;
        std::list<Key>::iterator it;
    };
    void detach(Key key, const FreqNode& node);  // unlink from its bucket

    std::unordered_map<Key, FreqNode> nodes_;
    std::map<int, std::list<Key>> buckets_;
    int minFreq_ = 0;
};

}  // namespace cachesim
