// Simulator.cpp
#include "Simulator.h"

#include <chrono>
#include <stdexcept>

namespace cachesim {

Simulator::Simulator(std::size_t capacity) : capacity_(capacity) {
    if (capacity_ == 0) throw std::invalid_argument("Simulator capacity must be at least 1");
}

Metrics Simulator::run(const std::vector<Key>& trace, std::unique_ptr<EvictionPolicy> policy,
                       CacheListener* observer, bool measureDecisions) const {
    Cache cache(capacity_, std::move(policy));
    cache.setMeasureDecisions(measureDecisions);
    MetricsRecorder recorder;
    cache.addListener(&recorder);
    if (observer != nullptr) cache.addListener(observer);

    const auto start = std::chrono::steady_clock::now();
    for (Key key : trace) cache.access(key);
    const auto stop = std::chrono::steady_clock::now();

    Metrics m = recorder.metrics();
    m.totalNs = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count();
    return m;  // recorder & cache are destroyed here (RAII) - listeners never dangle
}

}  // namespace cachesim
