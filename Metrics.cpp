// Metrics.cpp
#include "Metrics.h"

namespace cachesim {

double Metrics::hitRatio() const {
    return requests() == 0 ? 0.0 : static_cast<double>(hits) / static_cast<double>(requests());
}

double Metrics::nsPerRequest() const {
    return requests() == 0 ? 0.0 : static_cast<double>(totalNs) / static_cast<double>(requests());
}

double Metrics::avgDecisionNs() const {
    return evictions == 0 ? 0.0
                          : static_cast<double>(decisionNs) / static_cast<double>(evictions);
}

double Metrics::requestsPerSecond() const {
    return totalNs <= 0 ? 0.0 : static_cast<double>(requests()) * 1e9 / static_cast<double>(totalNs);
}

}  // namespace cachesim
