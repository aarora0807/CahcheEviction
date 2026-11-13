// Cache.h - fixed-capacity key/value store that delegates eviction to a policy.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "EvictionPolicy.h"

namespace cachesim {

class Cache;

/// [Design pattern: OBSERVER] Listeners are notified of cache events.
/// Default no-op bodies let a listener override only what it needs.
class CacheListener {
public:
    virtual ~CacheListener() = default;
    virtual void onHit(Key /*key*/) {}
    virtual void onMiss(Key /*key*/) {}
    virtual void onEvict(Key /*victim*/, std::int64_t /*decisionNs*/) {}
    /// Fired just before the policy is asked for a victim (cache still full).
    /// Lets a listener inspect the policy's view, e.g. the AI's scores.
    virtual void onBeforeEvict(const Cache& /*cache*/) {}
    /// Fired once per access() after all state changes (used by Tracer).
    virtual void onRequestComplete(const Cache& /*cache*/, Key /*key*/) {}
};

/// Owns the data (hash map) and the policy (unique_ptr -> RAII, no leaks).
/// Capacity is in entries. A capacity of 0 is rejected.
class Cache {
public:
    Cache(std::size_t capacity, std::unique_ptr<EvictionPolicy> policy);

    /// Read: on hit notifies the policy and returns the value.
    std::optional<Value> get(Key key);
    /// Write: updates in place, or inserts (evicting first if full).
    void put(Key key, Value value);
    /// Simulation helper: get(); on miss, load the value and put(). True on hit.
    bool access(Key key);

    /// Swap the algorithm at runtime (Strategy). The new policy is primed with
    /// the current residents (their relative order is not preserved).
    void setPolicy(std::unique_ptr<EvictionPolicy> policy);
    const EvictionPolicy& policy() const { return *policy_; }

    /// Non-owning: a listener must outlive the cache or be removed first.
    void addListener(CacheListener* listener);
    void removeListener(CacheListener* listener);

    /// Timing selectVictim() costs ~20-40 ns per eviction; it can be disabled.
    void setMeasureDecisions(bool on) { measureDecisions_ = on; }

    bool contains(Key key) const { return store_.count(key) != 0; }
    std::size_t size() const { return store_.size(); }
    std::size_t capacity() const { return capacity_; }
    /// Resident keys in ascending order (for tracing / tests).
    std::vector<Key> keys() const;

private:
    void evictOne();

    std::size_t capacity_;
    std::unordered_map<Key, Value> store_;
    std::unique_ptr<EvictionPolicy> policy_;
    std::vector<CacheListener*> listeners_;
    bool measureDecisions_ = true;
};

}  // namespace cachesim
