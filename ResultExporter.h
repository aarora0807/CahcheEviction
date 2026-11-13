// ResultExporter.h - console tables and CSV files.
#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "PolicyTrainer.h"
#include "Simulator.h"

namespace cachesim {

class ResultExporter {
public:
    /// policy | hits | misses | hit % | evictions | ns/request | gap to OPT,
    /// grouped by workload; '*' marks the best online (non-OPT) policy.
    static void printTable(std::ostream& out, const std::vector<SimulationResult>& results);
    /// results.csv : workload,policy,capacity,requests,hits,misses,hit_ratio,...
    static void writeCsv(const std::string& path, const std::vector<SimulationResult>& results);
    /// results_pivot.csv : one row per workload, one hit-ratio column per policy
    /// (ready for Excel/LibreOffice charts and scripts/plot.gp).
    static void writePivotCsv(const std::string& path, const std::vector<SimulationResult>& results);
    /// training_curve.csv : generation,best,mean,worst,sigma,w_recency,...
    static void writeTrainingCurve(const std::string& path,
                                   const std::vector<GenerationStats>& history);
};

}  // namespace cachesim
