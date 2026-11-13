// Cli.h - command-line parsing and the application front-end (menu + modes).
#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "AIPolicy.h"
#include "PolicyTrainer.h"
#include "Simulator.h"

namespace cachesim {

struct CliOptions {
    std::size_t capacity = 50;
    std::size_t requests = 100000;
    std::string workload = "all";  // a name from WorkloadFactory or "all"
    std::string policy = "all";    // a name from PolicyFactory or "all"
    std::uint64_t seed = 42;
    double alpha = 0.9;
    std::size_t universe = 1000;
    std::size_t phases = 2;
    std::string traceFile = "data/sample_trace.txt";
    std::string weightsPath = "weights.txt";
    std::string resultsDir = "results";
    std::size_t generations = 60;
    std::size_t population = 40;
    std::size_t threads = 0;
    std::size_t sampleK = 8;
    std::size_t traceSteps = 40;

    bool compareAll = false;
    bool traceMode = false;
    bool train = false;
    bool evaluate = false;
    bool benchmark = false;
    bool exportWeb = false;
    std::string webDir = "web";
    bool menu = false;
    bool help = false;
};

class Cli {
public:
    /// Throws std::invalid_argument with a helpful message on bad input.
    static CliOptions parse(const std::vector<std::string>& args);
    static void printUsage(std::ostream& out);
};

/// Front controller: each public mode is one use case of the simulator.
class App {
public:
    explicit App(CliOptions options);
    int run();  // dispatch on options; returns a process exit code

    int compare();    // every (workload x policy) -> table + results.csv
    int trace();      // step-by-step log for a short run
    int train();      // GA training -> weights.txt + training_curve.csv
    int evaluate();   // train-set vs unseen test-set hit ratios
    int benchmark();  // requests/second per policy
    int menu();       // interactive text menu
    int exportWeb();  // writes web/data.js for CacheSim Studio

    /// Training set A / test set B: same families, different seeds & params.
    static std::vector<TrainingCase> trainingSet(std::size_t capacity, std::size_t n,
                                                 std::uint64_t seed);
    static std::vector<TrainingCase> testSet(std::size_t capacity, std::size_t n,
                                             std::uint64_t seed, const std::string& traceFile);

private:
    AIWeights loadWeightsOrDefault(bool verbose) const;
    std::string resultPath(const std::string& file) const;
    std::vector<SimulationResult> runAllPolicies(const std::string& workloadName,
                                                 const std::vector<Key>& trace,
                                                 std::size_t capacity, const AIWeights& w,
                                                 bool measureDecisions = true) const;

    CliOptions opt_;
};

}  // namespace cachesim
