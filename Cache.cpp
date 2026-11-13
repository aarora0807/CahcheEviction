// Cache.cpp
#include "Cache.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>

namespace cachesim {

Cache::Cache(std::size_t capacity, std::unique_ptr<EvictionPolicy> policy)
    : capacity_(capacity), policy_(std::move(policy)) {
    if (capacity_ == 0) throw std::invalid_argument("Cache capacity must be at least 1");
    if (!policy_) throw std::invalid_argument("Cache requires a non-null eviction policy");
    store_.reserve(capacity_);
}

std::optional<Value> Cache::get(Key key) {
    auto it = store_.find(key);
    if (it == store_.end()) {
        for (CacheListener* l : listeners_) l->onMiss(key);
        return std::nullopt;
    }
    policy_->onAccess(key);
    for (CacheListener* l : listeners_) l->onHit(key);
    return it->second;
}

void Cache::put(Key key, Value value) {
    auto it = store_.find(key);
    if (it != store_.end()) {  // update in place counts as a use
        it->second = value;
        policy_->onAccess(key);
        return;
    }
    if (store_.size() >= capacity_) evictOne();
    store_.emplace(key, value);
    policy_->onInsert(key);
}

bool Cache::access(Key key) {
    const bool hit = get(key).has_value();
    if (!hit) put(key, key);  // "load from backing store": value = key
    for (CacheListener* l : listeners_) l->onRequestComplete(*this, key);
    return hit;
}

void Cache::evictOne() {
    for (CacheListener* l : listeners_) l->onBeforeEvict(*this);
    using Clock = std::chrono::steady_clock;
    std::int64_t ns = 0;
    Key victim = 0;
    if (measureDecisions_) {
        const auto t0 = Clock::now();
        victim = policy_->selectVictim();
        ns = std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count();
    } else {
        victim = policy_->selectVictim();
    }
    if (store_.erase(victim) == 0) {
        throw std::logic_error("policy " + policy_->name() + " chose a non-resident victim");
    }
    policy_->onRemove(victim);
    for (CacheListener* l : listeners_) l->onEvict(victim, ns);
}

void Cache::setPolicy(std::unique_ptr<EvictionPolicy> policy) {
    if (!policy) throw std::invalid_argument("Cache::setPolicy requires a non-null policy");
    for (Key k : keys()) policy->onInsert(k);
    policy_ = std::move(policy);
}

void Cache::addListener(CacheListener* listener) {
    if (listener != nullptr) listeners_.push_back(listener);
}

void Cache::removeListener(CacheListener* listener) {
    listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), listener),
                     listeners_.end());
}

std::vector<Key> Cache::keys() const {
    std::vector<Key> out;
    out.reserve(store_.size());
    for (const auto& kv : store_) out.push_back(kv.first);
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace cachesim
