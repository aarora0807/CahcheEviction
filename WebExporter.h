// WebExporter.h - writes web/data.js, the data file behind CacheSim Studio.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "AIPolicy.h"
#include "Simulator.h"

namespace cachesim {

/// A short request list that CacheSim Studio animates step by step.
struct WebScenario {
    std::string id;
    std::string title;
    std::string description;
    std::size_t capacity = 3;
    std::vector<Key> requests;
};

/// Collects results from the real C++ engine and writes them as a JavaScript
/// file (window.SIM_DATA = {...}). A <script> tag can load that file straight
/// from disk, so CacheSim Studio works by opening web/index.html, with
/// no web server and no internet connection.
class WebExporter {
public:
    explicit WebExporter(AIWeights weights);

    void setResults(const std::vector<SimulationResult>& results, std::size_t capacity,
                    std::size_t requests, std::uint64_t seed);
    /// Replays the scenario through every policy, recording each step (and,
    /// for the AI, every item's features and score before each eviction).
    void addScenario(const WebScenario& scenario);
    /// A real request trace used by the browser's AI Lab and pattern view,
    /// with the hits every policy gets on it (so the browser can check itself).
    void addWorkloadTrace(const std::string& name, const std::vector<Key>& trace,
                          std::size_t capacity);
    /// Reads training_curve.csv, generalization.csv and benchmark.csv if present.
    void loadResultCsvs(const std::string& dir);
    /// Throws std::runtime_error if the file cannot be written.
    void write(const std::string& path) const;

private:
    AIWeights weights_;
    std::string settings_ = "null";
    std::vector<std::string> results_, scenarios_, traces_, generalization_, benchmark_;
    std::string training_ = "null";
};

}  // namespace cachesim
