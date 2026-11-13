// Tracer.h - optional step-by-step log of a (small) simulation.
#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>

#include "Cache.h"

namespace cachesim {

/// Prints one line per request:
///   step | request | HIT/MISS | victim | cache contents (sorted)
class Tracer final : public CacheListener {
public:
    explicit Tracer(std::ostream& out, std::size_t maxLines = 200);

    void onHit(Key key) override;
    void onMiss(Key key) override;
    void onEvict(Key victim, std::int64_t decisionNs) override;
    void onRequestComplete(const Cache& cache, Key key) override;

private:
    std::ostream& out_;
    std::size_t maxLines_;
    std::size_t step_ = 0;
    bool lastHit_ = false;
    std::optional<Key> lastVictim_;
};

}  // namespace cachesim
