// LRUPolicy.cpp
#include "LRUPolicy.h"

#include <stdexcept>

namespace cachesim {

void LRUPolicy::onInsert(Key key) {
    if (where_.count(key) != 0) {
        onAccess(key);
        return;
    }
    order_.push_front(key);
    where_[key] = order_.begin();
}

void LRUPolicy::onAccess(Key key) {
    auto it = where_.find(key);
    if (it == where_.end()) return;
    // splice moves the node to the front in O(1); iterators stay valid.
    order_.splice(order_.begin(), order_, it->second);
}

void LRUPolicy::onRemove(Key key) {
    auto it = where_.find(key);
    if (it == where_.end()) return;
    order_.erase(it->second);
    where_.erase(it);
}

Key LRUPolicy::selectVictim() {
    if (order_.empty()) throw std::logic_error("LRUPolicy::selectVictim on empty policy");
    return order_.back();
}

}  // namespace cachesim
