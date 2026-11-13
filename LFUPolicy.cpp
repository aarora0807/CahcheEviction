// LFUPolicy.cpp
#include "LFUPolicy.h"

#include <stdexcept>

namespace cachesim {

void LFUPolicy::onInsert(Key key) {
    if (nodes_.count(key) != 0) {
        onAccess(key);
        return;
    }
    auto& bucket = buckets_[1];
    bucket.push_front(key);
    nodes_[key] = FreqNode{1, bucket.begin()};
    minFreq_ = 1;  // a brand-new key always has the lowest possible frequency
}

void LFUPolicy::detach(Key /*key*/, const FreqNode& node) {
    auto bucketIt = buckets_.find(node.freq);
    bucketIt->second.erase(node.it);
    if (bucketIt->second.empty()) buckets_.erase(bucketIt);
}

void LFUPolicy::onAccess(Key key) {
    auto it = nodes_.find(key);
    if (it == nodes_.end()) return;
    FreqNode& node = it->second;
    const int oldFreq = node.freq;
    detach(key, node);
    if (minFreq_ == oldFreq && buckets_.count(oldFreq) == 0) minFreq_ = oldFreq + 1;
    node.freq = oldFreq + 1;
    auto& bucket = buckets_[node.freq];
    bucket.push_front(key);  // most recent inside its bucket
    node.it = bucket.begin();
}

void LFUPolicy::onRemove(Key key) {
    auto it = nodes_.find(key);
    if (it == nodes_.end()) return;
    detach(key, it->second);
    nodes_.erase(it);
    // std::map keeps frequencies sorted, so the new minimum is begin().
    minFreq_ = buckets_.empty() ? 0 : buckets_.begin()->first;
}

Key LFUPolicy::selectVictim() {
    if (nodes_.empty()) throw std::logic_error("LFUPolicy::selectVictim on empty policy");
    return buckets_.at(minFreq_).back();  // least frequent, then least recent
}

int LFUPolicy::frequencyOf(Key key) const {
    auto it = nodes_.find(key);
    return it == nodes_.end() ? 0 : it->second.freq;
}

}  // namespace cachesim
