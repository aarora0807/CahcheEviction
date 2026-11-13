// BeladyPolicy.cpp
#include "BeladyPolicy.h"

#include <iterator>
#include <stdexcept>
#include <string>

namespace cachesim {

BeladyOptimalPolicy::BeladyOptimalPolicy(std::vector<Key> trace)
    : trace_(std::move(trace)), nextUse_(trace_.size(), kNever) {
    // One backwards pass: lastSeen[k] = nearest later position of key k.
    std::unordered_map<Key, std::size_t> lastSeen;
    lastSeen.reserve(trace_.size());
    for (std::size_t i = trace_.size(); i-- > 0;) {
        auto it = lastSeen.find(trace_[i]);
        nextUse_[i] = (it == lastSeen.end()) ? kNever : it->second;
        lastSeen[trace_[i]] = i;
    }
}

std::size_t BeladyOptimalPolicy::advance(Key key) {
    if (cursor_ >= trace_.size() || trace_[cursor_] != key) {
        throw std::logic_error("BeladyOptimalPolicy: request for key " + std::to_string(key) +
                               " does not match the trace it was built from");
    }
    return nextUse_[cursor_++];
}

void BeladyOptimalPolicy::onInsert(Key key) {
    const std::size_t next = advance(key);
    nextOf_[key] = next;
    byNextUse_.emplace(next, key);
}

void BeladyOptimalPolicy::onAccess(Key key) {
    const std::size_t next = advance(key);
    auto it = nextOf_.find(key);
    if (it == nextOf_.end()) return;
    byNextUse_.erase({it->second, key});
    it->second = next;
    byNextUse_.emplace(next, key);
}

void BeladyOptimalPolicy::onRemove(Key key) {
    auto it = nextOf_.find(key);
    if (it == nextOf_.end()) return;
    byNextUse_.erase({it->second, key});
    nextOf_.erase(it);
}

Key BeladyOptimalPolicy::selectVictim() {
    if (byNextUse_.empty()) throw std::logic_error("Belady::selectVictim on empty policy");
    return std::prev(byNextUse_.end())->second;  // farthest next use
}

}  // namespace cachesim
