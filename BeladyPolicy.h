// BeladyPolicy.h - Belady's offline optimal (OPT / MIN) policy.
#pragma once

#include <cstddef>
#include <set>
#include <unordered_map>
#include <utility>
#include <vector>

#include "EvictionPolicy.h"

namespace cachesim {

/// Evicts the resident key whose next use lies farthest in the future.
/// It needs the whole future trace, so it is NOT implementable online; we use
/// it only as the theoretical upper bound every real policy is measured against.
///
/// Preprocessing: one backwards pass computes nextUse[i] in O(n).
/// Runtime: an ordered set of (nextUse, key) gives the victim in O(log n).
/// Every onAccess/onInsert must match trace[cursor] (checked; throws otherwise).
class BeladyOptimalPolicy final : public EvictionPolicy {
public:
    explicit BeladyOptimalPolicy(std::vector<Key> trace);

    void onInsert(Key key) override;
    void onAccess(Key key) override;
    void onRemove(Key key) override;
    Key selectVictim() override;
    std::string name() const override { return "Belady-OPT"; }
    std::size_t trackedCount() const override { return nextOf_.size(); }

    /// Sentinel "next use" for keys that never appear again.
    static constexpr std::size_t kNever = static_cast<std::size_t>(-1);

private:
    std::size_t advance(Key key);  // consume one trace position, return next use

    std::vector<Key> trace_;
    std::vector<std::size_t> nextUse_;
    std::size_t cursor_ = 0;
    std::set<std::pair<std::size_t, Key>> byNextUse_;  // max element = victim
    std::unordered_map<Key, std::size_t> nextOf_;      // key -> its entry in set
};

}  // namespace cachesim
