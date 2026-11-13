// EvictionPolicy.h - the Strategy interface every replacement algorithm implements.
#pragma once

#include <cstddef>
#include <string>

#include "Types.h"

namespace cachesim {

/// [Design pattern: STRATEGY]
/// An EvictionPolicy only tracks *keys* and decides *which* key to evict.
/// The Cache owns the data; the policy owns the bookkeeping. This separation
/// lets us swap algorithms at runtime without touching the Cache.
///
/// Protocol (one call per request, issued by Cache):
///   hit                      -> onAccess(key)
///   miss, cache not full     -> onInsert(key)
///   miss, cache full         -> selectVictim(); onRemove(victim); onInsert(key)
class EvictionPolicy {
public:
    EvictionPolicy() = default;
    virtual ~EvictionPolicy() = default;

    // Polymorphic base: copying/moving through a base reference would slice,
    // so all copy/move operations are deleted (rule of five, explicitly).
    EvictionPolicy(const EvictionPolicy&) = delete;
    EvictionPolicy& operator=(const EvictionPolicy&) = delete;
    EvictionPolicy(EvictionPolicy&&) = delete;
    EvictionPolicy& operator=(EvictionPolicy&&) = delete;

    /// A new key was admitted into the cache.
    virtual void onInsert(Key key) = 0;
    /// A resident key was read or written (cache hit).
    virtual void onAccess(Key key) = 0;
    /// A key left the cache (eviction or explicit erase).
    virtual void onRemove(Key key) = 0;
    /// Choose the key to evict. Must NOT modify tracked state; the cache calls
    /// onRemove(victim) afterwards. Throws std::logic_error when empty.
    virtual Key selectVictim() = 0;
    /// Human-readable policy name used in tables and CSV files.
    virtual std::string name() const = 0;
    /// Number of keys the policy currently tracks (should equal cache size).
    virtual std::size_t trackedCount() const = 0;
};

}  // namespace cachesim
