// ResultExporter.cpp
#include "ResultExporter.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <ostream>
#include <stdexcept>

namespace cachesim {

namespace {
bool isOptimal(const std::string& policy) { return policy == "Belady-OPT"; }
}  // namespace

void ResultExporter::printTable(std::ostream& out, const std::vector<SimulationResult>& results) {
    // Preserve first-seen workload order.
    std::vector<std::string> workloads;
    for (const auto& r : results) {
        if (std::find(workloads.begin(), workloads.end(), r.workload) == workloads.end()) {
            workloads.push_back(r.workload);
        }
    }
    const auto flags = out.flags();
    for (const auto& w : workloads) {
        double best = -1.0, opt = -1.0;
        for (const auto& r : results) {
            if (r.workload != w) continue;
            if (isOptimal(r.policy)) opt = r.metrics.hitRatio();
            else best = std::max(best, r.metrics.hitRatio());
        }
        std::size_t cap = 0;
        for (const auto& r : results) if (r.workload == w) cap = r.capacity;

        out << "\nWorkload: " << w << "   (capacity " << cap << ")\n";
        out << "  " << std::left << std::setw(12) << "policy" << std::right << std::setw(10)
            << "hits" << std::setw(10) << "misses" << std::setw(9) << "hit %" << std::setw(11)
            << "evictions" << std::setw(12) << "ns/request" << std::setw(12) << "gap to OPT"
            << '\n';
        out << "  " << std::string(76, '-') << '\n';
        for (const auto& r : results) {
            if (r.workload != w) continue;
            const Metrics& m = r.metrics;
            const bool star = !isOptimal(r.policy) && m.hitRatio() >= best - 1e-12;
            out << (star ? "* " : "  ") << std::left << std::setw(12) << r.policy << std::right
                << std::setw(10) << m.hits << std::setw(10) << m.misses << std::fixed
                << std::setprecision(2) << std::setw(9) << 100.0 * m.hitRatio() << std::setw(11)
                << m.evictions << std::setprecision(1) << std::setw(12) << m.nsPerRequest();
            if (opt >= 0.0 && !isOptimal(r.policy)) {
                out << std::setprecision(2) << std::setw(11) << 100.0 * (opt - m.hitRatio()) << '%';
            } else {
                out << std::setw(12) << "-";
            }
            out << '\n';
        }
    }
    out.flags(flags);
}

void ResultExporter::writeCsv(const std::string& path,
                              const std::vector<SimulationResult>& results) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write " + path);
    out << "workload,policy,capacity,requests,hits,misses,evictions,hit_ratio,ns_per_request,"
           "avg_decision_ns,requests_per_sec\n";
    out << std::fixed;
    for (const auto& r : results) {
        const Metrics& m = r.metrics;
        out << '"' << r.workload << "\"," << r.policy << ',' << r.capacity << ',' << m.requests()
            << ',' << m.hits << ',' << m.misses << ',' << m.evictions << ','
            << std::setprecision(6) << m.hitRatio() << ',' << std::setprecision(2)
            << m.nsPerRequest() << ',' << m.avgDecisionNs() << ',' << std::setprecision(0)
            << m.requestsPerSecond() << '\n';
    }
}

void ResultExporter::writePivotCsv(const std::string& path,
                                   const std::vector<SimulationResult>& results) {
    std::vector<std::string> workloads, policies;
    for (const auto& r : results) {
        if (std::find(workloads.begin(), workloads.end(), r.workload) == workloads.end())
            workloads.push_back(r.workload);
        if (std::find(policies.begin(), policies.end(), r.policy) == policies.end())
            policies.push_back(r.policy);
    }
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write " + path);
    out << "workload";
    for (const auto& p : policies) out << ',' << p;
    out << '\n' << std::fixed << std::setprecision(4);
    for (const auto& w : workloads) {
        out << '"' << w << '"';
        for (const auto& p : policies) {
            out << ',';
            for (const auto& r : results)
                if (r.workload == w && r.policy == p) out << 100.0 * r.metrics.hitRatio();
        }
        out << '\n';
    }
}

void ResultExporter::writeTrainingCurve(const std::string& path,
                                        const std::vector<GenerationStats>& history) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write " + path);
    out << "generation,best,mean,worst,sigma";
    for (const char* f : AIWeights::featureNames()) out << ",w_" << f;
    out << '\n' << std::fixed << std::setprecision(6);
    for (const auto& g : history) {
        out << g.generation << ',' << g.best << ',' << g.mean << ',' << g.worst << ',' << g.sigma;
        for (double w : g.bestWeights) out << ',' << w;
        out << '\n';
    }
}

}  // namespace cachesim
