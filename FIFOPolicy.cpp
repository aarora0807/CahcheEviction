// FIFOPolicy.cpp
#include "FIFOPolicy.h"

#include <iterator>
#include <stdexcept>

namespace cachesim {

void FIFOPolicy::onInsert(Key key) {
    if (where_.count(key) != 0) return;  // already tracked: insertion time unchanged
    queue_.push_back(key);
    where_[key] = std::prev(queue_.end());
}

void FIFOPolicy::onAccess(Key /*key*/) {
    // Intentionally empty: FIFO's order depends only on insertion time.
}

void FIFOPolicy::onRemove(Key key) {
    auto it = where_.find(key);
    if (it == where_.end()) return;
    queue_.erase(it->second);
    where_.erase(it);
}

Key FIFOPolicy::selectVictim() {
    if (queue_.empty()) throw std::logic_error("FIFOPolicy::selectVictim on empty policy");
    return queue_.front();
}

}  // namespace cachesim
