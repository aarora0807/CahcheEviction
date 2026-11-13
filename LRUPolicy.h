// LRUPolicy.h - Least Recently Used replacement.
#pragma once

#include <list>
#include <unordered_map>

#include "EvictionPolicy.h"

namespace cachesim {

/// Evicts the key whose last access is oldest.
/// list ordered MRU (front) -> LRU (back) + hash map of iterators.
/// list::splice relinks a node without allocation, so access is O(1).
class LRUPolicy final : public EvictionPolicy {
public:
    void onInsert(Key key) override;
    void onAccess(Key key) override;
    void onRemove(Key key) override;
    Key selectVictim() override;
    std::string name() const override { return "LRU"; }
    std::size_t trackedCount() const override { return order_.size(); }

private:
    std::list<Key> order_;                                      // front = MRU
    std::unordered_map<Key, std::list<Key>::iterator> where_;
};

}  // namespace cachesim
