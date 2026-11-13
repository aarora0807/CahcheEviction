// Metrics.h - counters collected during one simulation run.
#pragma once

#include <cstdint>

#include "Cache.h"

namespace cachesim {

/// Plain value type (rule of zero): copyable, comparable, cheap.
struct Metrics {
    std::uint64_t hits = 0;
    std::uint64_t misses = 0;
    std::uint64_t evictions = 0;
    std::int64_t totalNs = 0;     // wall time of the whole run
    std::int64_t decisionNs = 0;  // time spent inside selectVictim()

    std::uint64_t requests() const { return hits + misses; }
    double hitRatio() const;
    double nsPerRequest() const;
    double avgDecisionNs() const;
    double requestsPerSecond() const;
};

/// [Design pattern: OBSERVER] - the concrete listener that fills a Metrics.
class MetricsRecorder final : public CacheListener {
public:
    void onHit(Key) override { ++metrics_.hits; }
    void onMiss(Key) override { ++metrics_.misses; }
    void onEvict(Key, std::int64_t decisionNs) override {
        ++metrics_.evictions;
        metrics_.decisionNs += decisionNs;
    }
    const Metrics& metrics() const { return metrics_; }
    Metrics& metrics() { return metrics_; }

private:
    Metrics metrics_;
};

}  // namespace cachesim
