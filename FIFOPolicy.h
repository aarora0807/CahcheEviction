// FIFOPolicy.h - First-In First-Out replacement.
#pragma once

#include <list>
#include <unordered_map>

#include "EvictionPolicy.h"

namespace cachesim {

/// Evicts the key that entered the cache earliest, ignoring later accesses.
/// list (front = oldest) + hash map of iterators -> every operation is O(1).
class FIFOPolicy final : public EvictionPolicy {
public:
    void onInsert(Key key) override;
    void onAccess(Key key) override;  // no-op: FIFO ignores reuse
    void onRemove(Key key) override;
    Key selectVictim() override;
    std::string name() const override { return "FIFO"; }
    std::size_t trackedCount() const override { return queue_.size(); }

private:
    std::list<Key> queue_;                                      // front = oldest
    std::unordered_map<Key, std::list<Key>::iterator> where_;  // key -> node
};

}  // namespace cachesim
