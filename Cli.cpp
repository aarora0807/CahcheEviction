// Cli.cpp - argument parsing + the App use cases (compare, trace, train, ...).
#include "Cli.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "PolicyFactory.h"
#include "ResultExporter.h"
#include "Tracer.h"
#include "WebExporter.h"
#include "Workloads.h"

namespace cachesim {

// ======================================================================= Cli
namespace {

std::size_t parseCount(const std::string& flag, const std::string& v) {
    try {
        std::size_t used = 0;
        if (!v.empty() && v[0] == '-') throw std::invalid_argument("negative");
        const unsigned long long x = std::stoull(v, &used);
        if (used != v.size()) throw std::invalid_argument("trailing characters");
        return static_cast<std::size_t>(x);
    } catch (const std::exception&) {
        throw std::invalid_argument(flag + " expects a non-negative integer, got '" + v + "'");
    }
}

double parseReal(const std::string& flag, const std::string& v) {
    try {
        std::size_t used = 0;
        const double x = std::stod(v, &used);
        if (used != v.size()) throw std::invalid_argument("trailing characters");
        return x;
    } catch (const std::exception&) {
        throw std::invalid_argument(flag + " expects a number, got '" + v + "'");
    }
}

bool fileExists(const std::string& path) { return static_cast<bool>(std::ifstream(path)); }

}  // namespace

CliOptions Cli::parse(const std::vector<std::string>& args) {
    CliOptions o;
    bool anyMode = false;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        auto value = [&]() -> const std::string& {
            if (i + 1 >= args.size()) throw std::invalid_argument(a + " needs a value");
            return args[++i];
        };
        if (a == "--help" || a == "-h") { o.help = true; anyMode = true; }
        else if (a == "--compare-all") { o.compareAll = true; anyMode = true; }
        else if (a == "--trace") { o.traceMode = true; anyMode = true; }
        else if (a == "--train") { o.train = true; anyMode = true; }
        else if (a == "--evaluate") { o.evaluate = true; anyMode = true; }
        else if (a == "--benchmark") { o.benchmark = true; anyMode = true; }
        else if (a == "--menu") { o.menu = true; anyMode = true; }
        else if (a == "--export-web") { o.exportWeb = true; anyMode = true; }
        else if (a == "--web-dir") o.webDir = value();
        else if (a == "--capacity" || a == "-c") o.capacity = parseCount(a, value());
        else if (a == "--n" || a == "--requests" || a == "-n") o.requests = parseCount(a, value());
        else if (a == "--workload" || a == "-w") o.workload = value();
        else if (a == "--policy" || a == "-p") o.policy = value();
        else if (a == "--seed") o.seed = parseCount(a, value());
        else if (a == "--alpha") o.alpha = parseReal(a, value());
        else if (a == "--universe") o.universe = parseCount(a, value());
        else if (a == "--phases") o.phases = parseCount(a, value());
        else if (a == "--trace-file") o.traceFile = value();
        else if (a == "--weights") o.weightsPath = value();
        else if (a == "--results-dir") o.resultsDir = value();
        else if (a == "--generations") o.generations = parseCount(a, value());
        else if (a == "--population") o.population = parseCount(a, value());
        else if (a == "--threads") o.threads = parseCount(a, value());
        else if (a == "--sample-k") o.sampleK = parseCount(a, value());
        else if (a == "--steps") o.traceSteps = parseCount(a, value());
        else throw std::invalid_argument("unknown option '" + a + "' (see --help)");
    }
    if (o.capacity == 0) throw std::invalid_argument("--capacity must be at least 1");
    if (o.universe == 0) throw std::invalid_argument("--universe must be at least 1");
    if (args.empty()) o.menu = true;          // no arguments -> interactive menu
    else if (!anyMode) o.compareAll = true;   // only parameters -> default mode
    return o;
}

void Cli::printUsage(std::ostream& out) {
    out << "Cache Eviction Simulator with an Evolved (AI) Eviction Policy\n\n"
           "usage: cache_sim [mode] [options]      (no arguments = interactive menu)\n\n"
           "modes:\n"
           "  --compare-all        every policy on every workload -> table + results.csv\n"
           "  --trace              step-by-step log of a short run (try --capacity 3)\n"
           "  --train              evolve AI weights (GA) -> weights.txt, training_curve.csv\n"
           "  --evaluate           train-set vs unseen test-set hit ratios\n"
           "  --benchmark          requests/second per policy at several cache sizes\n"
           "  --menu               interactive menu\n"
           "  --export-web         write web/data.js for CacheSim Studio (web/index.html)\n\n"
           "options:\n"
           "  --capacity N         cache size in entries (default 50)\n"
           "  --n N                requests per workload (default 100000)\n"
           "  --workload W         sequential|uniform|zipf|looping|scan|phase|trace|all\n"
           "  --policy P           fifo|lru|lfu|ai|ai-sampled|adaptive|belady|all\n"
           "  --seed S             RNG seed (default 42)\n"
           "  --alpha A            Zipf skew (default 0.9)\n"
           "  --universe U         number of distinct keys (default 1000)\n"
           "  --phases P           hot-set changes for 'phase' (default 2)\n"
           "  --trace-file F       key trace for 'trace' (default data/sample_trace.txt)\n"
           "  --weights F          AI weights file (default weights.txt)\n"
           "  --results-dir D      where CSVs go (default results)\n"
           "  --generations G      GA generations (default 60)\n"
           "  --population P       GA population (default 40)\n"
           "  --threads T          fitness threads (0 = all cores)\n"
           "  --sample-k K         candidates for ai-sampled (default 8)\n"
           "  --steps N            requests shown in --trace (default 40)\n"
           "  --web-dir D          where --export-web writes data.js (default web)\n";
}

// ======================================================================= App
App::App(CliOptions options) : opt_(std::move(options)) {}

int App::run() {
    if (opt_.help) {
        Cli::printUsage(std::cout);
        return 0;
    }
    if (opt_.menu) return menu();
    int rc = 0;
    if (opt_.train) rc |= train();
    if (opt_.evaluate) rc |= evaluate();
    if (opt_.compareAll) rc |= compare();
    if (opt_.traceMode) rc |= trace();
    if (opt_.benchmark) rc |= benchmark();
    if (opt_.exportWeb) rc |= exportWeb();
    return rc;
}

std::string App::resultPath(const std::string& file) const {
    return opt_.resultsDir.empty() ? file : opt_.resultsDir + "/" + file;
}

AIWeights App::loadWeightsOrDefault(bool verbose) const {
    try {
        AIWeights w = AIWeights::load(opt_.weightsPath);
        if (verbose) std::cout << "AI weights loaded from " << opt_.weightsPath << ": "
                               << w.toString() << '\n';
        return w;
    } catch (const std::exception&) {
        if (verbose) std::cout << "No trained weights at '" << opt_.weightsPath
                               << "' - using defaults (run --train first): "
                               << AIWeights::defaults().toString() << '\n';
        return AIWeights::defaults();
    }
}

std::vector<SimulationResult> App::runAllPolicies(const std::string& workloadName,
                                                  const std::vector<Key>& trace,
                                                  std::size_t capacity, const AIWeights& w,
                                                  bool measureDecisions) const {
    PolicyConfig cfg;
    cfg.capacity = capacity;
    cfg.futureTrace = &trace;
    cfg.aiWeights = w;
    cfg.sampleK = opt_.sampleK;

    std::vector<std::string> names = PolicyFactory::available();
    if (opt_.policy != "all") names = {opt_.policy};

    const Simulator sim(capacity);
    std::vector<SimulationResult> out;
    for (const std::string& n : names) {
        auto policy = PolicyFactory::create(n, cfg);
        const std::string label = policy->name();
        out.push_back({workloadName, label, capacity,
                       sim.run(trace, std::move(policy), nullptr, measureDecisions)});
    }
    return out;
}

int App::compare() {
    std::cout << "\n=== Policy comparison: capacity " << opt_.capacity << ", " << opt_.requests
              << " requests/workload, seed " << opt_.seed << " ===\n";
    const AIWeights w = loadWeightsOrDefault(true);

    WorkloadParams p;
    p.capacity = opt_.capacity;
    p.universe = opt_.universe;
    p.alpha = opt_.alpha;
    p.phases = opt_.phases;
    p.seed = opt_.seed;
    p.traceFile = opt_.traceFile;

    std::vector<std::string> workloads = WorkloadFactory::available();
    if (opt_.workload != "all") workloads = {opt_.workload};
    else if (fileExists(opt_.traceFile)) workloads.push_back("trace");

    std::vector<SimulationResult> all;
    for (const std::string& name : workloads) {
        auto gen = WorkloadFactory::create(name, p);
        const std::size_t n = (name == "trace") ? 0 : opt_.requests;
        const std::vector<Key> trace = gen->generate(n);
        auto rows = runAllPolicies(gen->name(), trace, opt_.capacity, w);
        all.insert(all.end(), rows.begin(), rows.end());
    }
    ResultExporter::printTable(std::cout, all);
    try {
        ResultExporter::writeCsv(resultPath("results.csv"), all);
        ResultExporter::writePivotCsv(resultPath("results_pivot.csv"), all);
        std::cout << "\nWrote " << resultPath("results.csv") << " and "
                  << resultPath("results_pivot.csv") << '\n';
    } catch (const std::exception& e) {
        std::cerr << "warning: " << e.what() << '\n';
    }
    return 0;
}

int App::trace() {
    std::vector<Key> keys;
    std::string label;
    if (opt_.workload == "all") {
        keys = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};  // Belady's-anomaly reference string
        label = "classic reference string";
    } else {
        WorkloadParams p;
        p.capacity = opt_.capacity;
        p.universe = std::min<std::size_t>(opt_.universe, opt_.capacity * 3 + 1);
        p.alpha = opt_.alpha;
        p.seed = opt_.seed;
        p.traceFile = opt_.traceFile;
        auto gen = WorkloadFactory::create(opt_.workload, p);
        keys = gen->generate(opt_.traceSteps);
        label = gen->name();
    }
    PolicyConfig cfg;
    cfg.capacity = opt_.capacity;
    cfg.futureTrace = &keys;
    cfg.aiWeights = loadWeightsOrDefault(false);
    cfg.sampleK = opt_.sampleK;

    std::vector<std::string> names = PolicyFactory::available();
    if (opt_.policy != "all") names = {opt_.policy};
    const Simulator sim(opt_.capacity);
    for (const std::string& n : names) {
        auto policy = PolicyFactory::create(n, cfg);
        std::cout << "\n--- " << policy->name() << " | capacity " << opt_.capacity << " | "
                  << label << " ---\n";
        Tracer tracer(std::cout, opt_.traceSteps);
        const Metrics m = sim.run(keys, std::move(policy), &tracer);
        std::cout << "hits=" << m.hits << " misses=" << m.misses << " evictions=" << m.evictions
                  << " hit ratio=" << std::fixed << std::setprecision(2) << 100.0 * m.hitRatio()
                  << "%\n";
        std::cout.unsetf(std::ios::fixed);
    }
    return 0;
}

std::vector<TrainingCase> App::trainingSet(std::size_t cap, std::size_t n, std::uint64_t seed) {
    // Set A: a mix of families so the GA cannot overfit a single pattern.
    std::vector<TrainingCase> cases;
    auto add = [&](const std::string& name, WorkloadGenerator& g) {
        cases.push_back({name, g.generate(n), cap});
    };
    ZipfWorkload zipf(1000, 0.8, seed);
    ScanResistanceWorkload scan(cap * 4 / 5 + 1, 500, cap * 2, seed + 1);
    PhaseShiftWorkload phase(1000, cap * 4 / 5 + 1, 2, 0.9, seed + 2);
    LoopingWorkload loop(cap + cap / 5 + 1, seed + 3);
    SequentialWorkload seq(600, 16, seed + 4);
    add("zipf(0.8)", zipf);
    add("scan", scan);
    add("phase", phase);
    add("looping", loop);
    add("sequential", seq);
    return cases;
}

std::vector<TrainingCase> App::testSet(std::size_t cap, std::size_t n, std::uint64_t seed,
                                       const std::string& traceFile) {
    // Set B: unseen seeds AND shifted parameters (harder than a pure re-seed).
    std::vector<TrainingCase> cases;
    auto add = [&](const std::string& name, WorkloadGenerator& g, std::size_t len) {
        cases.push_back({name, g.generate(len), cap});
    };
    ZipfWorkload zipf(2000, 1.1, seed);
    ScanResistanceWorkload scan(cap * 3 / 5 + 1, 300, cap * 3, seed + 1);
    PhaseShiftWorkload phase(1500, cap * 4 / 5 + 1, 3, 0.85, seed + 2);
    LoopingWorkload loop(cap + cap * 3 / 10 + 1, seed + 3);
    UniformRandomWorkload uni(cap * 4, seed + 4);
    add("zipf(1.1)", zipf, n);
    add("scan'", scan, n);
    add("phase x3", phase, n);
    add("looping+30%", loop, n);
    add("uniform", uni, n);
    if (fileExists(traceFile)) {
        TraceFileWorkload file(traceFile);
        add("trace file", file, 0);
    }
    return cases;
}

int App::train() {
    const std::size_t n = std::min<std::size_t>(opt_.requests, 20000);
    std::cout << "\n=== Training AIPolicy with a genetic algorithm ===\n"
              << "population " << opt_.population << ", generations " << opt_.generations
              << ", capacity " << opt_.capacity << ", " << n << " requests x 5 workloads"
              << " (training set A, seed 1001)\n";
    TrainerConfig cfg;
    cfg.populationSize = opt_.population;
    cfg.generations = opt_.generations;
    cfg.threads = opt_.threads;
    PolicyTrainer trainer(cfg, trainingSet(opt_.capacity, n, 1001));

    const AIWeights best = trainer.train([](const GenerationStats& g) {
        std::cout << "gen " << std::setw(3) << g.generation << "  best " << std::fixed
                  << std::setprecision(4) << g.best << "  mean " << g.mean << "  worst "
                  << g.worst << "  sigma " << std::setprecision(3) << g.sigma << '\n';
        std::cout.unsetf(std::ios::fixed);
    });
    std::cout << "\nBest weights: " << best.toString() << '\n';
    try {
        best.save(opt_.weightsPath);
        ResultExporter::writeTrainingCurve(resultPath("training_curve.csv"), trainer.history());
        std::cout << "Saved " << opt_.weightsPath << " and " << resultPath("training_curve.csv")
                  << '\n';
    } catch (const std::exception& e) {
        std::cerr << "warning: " << e.what() << '\n';
    }
    return 0;
}

int App::evaluate() {
    const std::size_t n = std::min<std::size_t>(opt_.requests, 20000);
    const AIWeights w = loadWeightsOrDefault(true);
    const auto setA = trainingSet(opt_.capacity, n, 1001);
    const auto setB = testSet(opt_.capacity, n, 2002, opt_.traceFile);

    std::cout << "\n=== Generalisation: train set A (seed 1001) vs unseen test set B (seed 2002) "
                 "===\n";
    std::vector<SimulationResult> rows;
    struct Summary { std::string policy; double train = 0, test = 0; };
    std::vector<Summary> summary;
    for (const auto* set : {&setA, &setB}) {
        for (std::size_t i = 0; i < set->size(); ++i) {
            const TrainingCase& c = (*set)[i];
            const std::string tag = (set == &setA ? "A: " : "B: ") + c.name;
            auto r = runAllPolicies(tag, c.trace, c.capacity, w, false);
            for (const auto& row : r) {
                auto it = std::find_if(summary.begin(), summary.end(),
                                       [&](const Summary& s) { return s.policy == row.policy; });
                if (it == summary.end()) {
                    summary.push_back({row.policy, 0, 0});
                    it = std::prev(summary.end());
                }
                (set == &setA ? it->train : it->test) +=
                    row.metrics.hitRatio() / static_cast<double>(set->size());
            }
            rows.insert(rows.end(), r.begin(), r.end());
        }
    }
    ResultExporter::printTable(std::cout, rows);

    std::cout << "\nMean hit ratio      train (A)   test (B)\n";
    std::ofstream csv(resultPath("generalization.csv"));
    if (csv) csv << "policy,train_hit_ratio,test_hit_ratio\n";
    for (const auto& s : summary) {
        std::cout << "  " << std::left << std::setw(14) << s.policy << std::right << std::fixed
                  << std::setprecision(2) << std::setw(9) << 100.0 * s.train << "%"
                  << std::setw(10) << 100.0 * s.test << "%\n";
        if (csv) csv << s.policy << ',' << std::setprecision(6) << s.train << ',' << s.test << '\n';
    }
    std::cout.unsetf(std::ios::fixed);
    try {
        ResultExporter::writeCsv(resultPath("evaluation.csv"), rows);
        std::cout << "Wrote " << resultPath("generalization.csv") << " and "
                  << resultPath("evaluation.csv") << '\n';
    } catch (const std::exception& e) {
        std::cerr << "warning: " << e.what() << '\n';
    }
    return 0;
}

int App::benchmark() {
    const std::size_t n = std::max<std::size_t>(opt_.requests, 200000);
    const AIWeights w = loadWeightsOrDefault(false);
    std::cout << "\n=== Throughput benchmark: zipf(0.9), " << n
              << " requests, universe = 16 x capacity ===\n";
    std::cout << "  " << std::left << std::setw(12) << "policy" << std::right;
    const std::vector<std::size_t> caps{64, 512, 4096};
    for (std::size_t c : caps) std::cout << std::setw(16) << ("cap " + std::to_string(c));
    std::cout << "   (million requests / second)\n";

    std::vector<std::vector<SimulationResult>> perCap;
    for (std::size_t c : caps) {
        ZipfWorkload z(c * 16, 0.9, opt_.seed);
        perCap.push_back(runAllPolicies("zipf bench cap " + std::to_string(c), z.generate(n), c, w,
                                        false));
    }
    std::ofstream csv(resultPath("benchmark.csv"));
    if (csv) csv << "policy,capacity,requests,ns_per_request,requests_per_sec,hit_ratio\n";
    for (std::size_t p = 0; p < perCap.front().size(); ++p) {
        std::cout << "  " << std::left << std::setw(12) << perCap.front()[p].policy << std::right;
        for (std::size_t c = 0; c < caps.size(); ++c) {
            const Metrics& m = perCap[c][p].metrics;
            std::cout << std::fixed << std::setprecision(2) << std::setw(16)
                      << m.requestsPerSecond() / 1e6;
            if (csv) csv << perCap[c][p].policy << ',' << caps[c] << ',' << m.requests() << ','
                         << m.nsPerRequest() << ',' << std::setprecision(0)
                         << m.requestsPerSecond() << ',' << std::setprecision(6) << m.hitRatio()
                         << '\n';
        }
        std::cout << '\n';
    }
    std::cout.unsetf(std::ios::fixed);
    std::cout << "Wrote " << resultPath("benchmark.csv") << '\n';
    return 0;
}

int App::menu() {
    for (;;) {
        std::cout << "\n========= Cache Eviction Simulator =========\n"
                  << " capacity=" << opt_.capacity << "  requests=" << opt_.requests
                  << "  seed=" << opt_.seed << "  workload=" << opt_.workload
                  << "  policy=" << opt_.policy << "\n"
                  << " 1) Compare policies\n 2) Step-by-step trace\n 3) Train AI policy (GA)\n"
                  << " 4) Evaluate generalisation (train vs test)\n 5) Benchmark throughput\n"
                  << " 6) Change settings\n 7) Export data for CacheSim Studio\n 0) Exit\n> ";
        std::string choice;
        if (!(std::cin >> choice) || choice == "0") return 0;
        try {
            if (choice == "1") compare();
            else if (choice == "2") trace();
            else if (choice == "3") train();
            else if (choice == "4") evaluate();
            else if (choice == "5") benchmark();
            else if (choice == "7") exportWeb();
            else if (choice == "6") {
                std::string cap, req, seed, wl, pol;
                std::cout << "capacity (" << opt_.capacity << "): ";
                std::cin >> cap;
                std::cout << "requests (" << opt_.requests << "): ";
                std::cin >> req;
                std::cout << "seed (" << opt_.seed << "): ";
                std::cin >> seed;
                std::cout << "workload [all|sequential|uniform|zipf|looping|scan|phase|trace]: ";
                std::cin >> wl;
                std::cout << "policy [all|fifo|lru|lfu|ai|ai-sampled|adaptive|belady]: ";
                std::cin >> pol;
                if (!std::cin) return 0;
                const std::size_t c = parseCount("capacity", cap);
                if (c == 0) throw std::invalid_argument("capacity must be at least 1");
                opt_.capacity = c;
                opt_.requests = parseCount("requests", req);
                opt_.seed = parseCount("seed", seed);
                opt_.workload = wl;
                opt_.policy = pol;
            } else {
                std::cout << "unknown choice\n";
            }
        } catch (const std::exception& e) {
            std::cout << "error: " << e.what() << '\n';
        }
    }
}

int App::exportWeb() {
    std::cout << "\n=== Exporting data for CacheSim Studio ===\n";
    const AIWeights w = loadWeightsOrDefault(true);
    WebExporter ex(w);

    // 1. Full comparison (same as --compare-all, without the table).
    WorkloadParams p;
    p.capacity = opt_.capacity;
    p.universe = opt_.universe;
    p.alpha = opt_.alpha;
    p.phases = opt_.phases;
    p.seed = opt_.seed;
    p.traceFile = opt_.traceFile;
    std::vector<std::string> workloads = WorkloadFactory::available();
    if (fileExists(opt_.traceFile)) workloads.push_back("trace");
    std::vector<SimulationResult> all;
    for (const std::string& name : workloads) {
        auto gen = WorkloadFactory::create(name, p);
        const std::vector<Key> trace = gen->generate(name == "trace" ? 0 : opt_.requests);
        auto rows = runAllPolicies(gen->name(), trace, opt_.capacity, w);
        all.insert(all.end(), rows.begin(), rows.end());
        // Short real traces for the browser's AI Lab and pattern explorer.
        if (name != "trace") ex.addWorkloadTrace(gen->name(), gen->generate(8000), opt_.capacity);
        std::cout << "  simulated " << gen->name() << '\n';
    }
    ex.setResults(all, opt_.capacity, opt_.requests, opt_.seed);

    // 2. Small step-by-step scenarios for the animated "Live Race".
    ex.addScenario({"classic", "Classic example", "Belady's famous reference string.", 3,
                    {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5}});
    std::vector<Key> loop;
    for (int r = 0; r < 6; ++r) for (Key k = 1; k <= 5; ++k) loop.push_back(k);
    ex.addScenario({"loop", "Loop bigger than the cache", "Keys 1-5 again and again with room for 4: LRU never hits.", 4, loop});
    ex.addScenario({"scan", "One-time scan", "A hot set (1, 2, 3), then a scan of new keys, then the hot set again.", 4,
                    {1, 2, 3, 1, 2, 3, 1, 2, 3, 10, 11, 12, 13, 14, 15, 1, 2, 3, 1, 2, 3}});
    ex.addScenario({"phase", "Hot set moves", "1, 2, 3 are popular, then 7, 8, 9 take over. LFU is slow to forget.", 4,
                    {1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 7, 8, 9, 7, 8, 9, 7, 8, 9, 7, 8, 9}});
    ZipfWorkload z(30, 1.0, 7);
    ex.addScenario({"zipf", "Popular keys (Zipf)", "A few keys are very popular, most are rare.", 5, z.generate(60)});

    // 3. Results produced by --train, --evaluate and --benchmark, if present.
    ex.loadResultCsvs(opt_.resultsDir);

    const std::string path = opt_.webDir + "/data.js";
    try {
        ex.write(path);
        std::cout << "Wrote " << path << " - open CacheSim Studio.bat (or " << opt_.webDir << "/index.html).\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    return 0;
}

}  // namespace cachesim
