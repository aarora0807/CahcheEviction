// Workloads.cpp - concrete generators + WorkloadFactory.
#include "Workloads.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace cachesim {

namespace {
std::size_t atLeastOne(std::size_t v) { return v == 0 ? 1 : v; }
}  // namespace

// ------------------------------------------------------------- Sequential
SequentialWorkload::SequentialWorkload(std::size_t universe, std::size_t runLength,
                                       std::uint64_t seed)
    : WorkloadGenerator(seed), universe_(atLeastOne(universe)), runLength_(atLeastOne(runLength)) {}

void SequentialWorkload::prepare(std::size_t) { runStart_ = 0; }

Key SequentialWorkload::nextKey(std::size_t i, std::size_t) {
    if (i % runLength_ == 0) {  // start a new run at a random offset
        std::uniform_int_distribution<std::size_t> start(0, universe_ - 1);
        runStart_ = static_cast<Key>(start(rng()));
    }
    const std::size_t offset = i % runLength_;
    return static_cast<Key>((static_cast<std::size_t>(runStart_) + offset) % universe_);
}

// ---------------------------------------------------------------- Uniform
UniformRandomWorkload::UniformRandomWorkload(std::size_t universe, std::uint64_t seed)
    : WorkloadGenerator(seed), universe_(atLeastOne(universe)) {}

Key UniformRandomWorkload::nextKey(std::size_t, std::size_t) {
    std::uniform_int_distribution<std::size_t> dist(0, universe_ - 1);
    return static_cast<Key>(dist(rng()));
}

// ------------------------------------------------------------------- Zipf
ZipfWorkload::ZipfWorkload(std::size_t universe, double alpha, std::uint64_t seed)
    : WorkloadGenerator(seed), alpha_(alpha), cdf_(atLeastOne(universe)) {
    double sum = 0.0;
    for (std::size_t k = 0; k < cdf_.size(); ++k) {
        sum += 1.0 / std::pow(static_cast<double>(k + 1), alpha_);
        cdf_[k] = sum;
    }
    for (double& c : cdf_) c /= sum;  // normalise to a proper CDF
    cdf_.back() = 1.0;
}

std::string ZipfWorkload::name() const {
    std::ostringstream ss;
    ss << "zipf(" << alpha_ << ")";
    return ss.str();
}

Key ZipfWorkload::nextKey(std::size_t, std::size_t) {
    std::uniform_real_distribution<double> u(0.0, 1.0);
    const double x = u(rng());
    // Inverse-CDF sampling: O(log N) binary search; rank 0 is the hottest key.
    const auto it = std::lower_bound(cdf_.begin(), cdf_.end(), x);
    return static_cast<Key>(it - cdf_.begin());
}

// ---------------------------------------------------------------- Looping
LoopingWorkload::LoopingWorkload(std::size_t loopLength, std::uint64_t seed)
    : WorkloadGenerator(seed), loopLength_(atLeastOne(loopLength)) {}

Key LoopingWorkload::nextKey(std::size_t i, std::size_t) {
    return static_cast<Key>(i % loopLength_);
}

// ---------------------------------------------------------- ScanResistance
ScanResistanceWorkload::ScanResistanceWorkload(std::size_t hotSetSize, std::size_t hotBurst,
                                               std::size_t scanLength, std::uint64_t seed)
    : WorkloadGenerator(seed),
      hotSetSize_(atLeastOne(hotSetSize)),
      hotBurst_(atLeastOne(hotBurst)),
      scanLength_(scanLength) {}

void ScanResistanceWorkload::prepare(std::size_t) { nextScanKey_ = 1000000; }

Key ScanResistanceWorkload::nextKey(std::size_t i, std::size_t) {
    const std::size_t period = hotBurst_ + scanLength_;
    if (i % period < hotBurst_) {
        std::uniform_int_distribution<std::size_t> hot(0, hotSetSize_ - 1);
        return static_cast<Key>(hot(rng()));
    }
    return nextScanKey_++;  // one-time key, never requested again
}

// ------------------------------------------------------------- PhaseShift
PhaseShiftWorkload::PhaseShiftWorkload(std::size_t universe, std::size_t hotSetSize,
                                       std::size_t phases, double hotProbability,
                                       std::uint64_t seed)
    : WorkloadGenerator(seed),
      universe_(atLeastOne(universe)),
      hotSetSize_(atLeastOne(hotSetSize)),
      phases_(atLeastOne(phases)),
      hotProbability_(hotProbability) {}

Key PhaseShiftWorkload::nextKey(std::size_t i, std::size_t n) {
    const std::size_t phase = (i * phases_) / (n == 0 ? 1 : n);
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    if (coin(rng()) < hotProbability_) {
        // Each phase's hot set is a disjoint block of keys.
        std::uniform_int_distribution<std::size_t> hot(0, hotSetSize_ - 1);
        return static_cast<Key>((phase * hotSetSize_ + hot(rng())) % universe_);
    }
    std::uniform_int_distribution<std::size_t> any(0, universe_ - 1);
    return static_cast<Key>(any(rng()));
}

// -------------------------------------------------------------- TraceFile
TraceFileWorkload::TraceFileWorkload(const std::string& path) : WorkloadGenerator(0) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open trace file: " + path);
    std::string line;
    while (std::getline(in, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::replace(line.begin(), line.end(), ',', ' ');
        std::istringstream ss(line);
        std::string token;
        while (ss >> token) {
            try {
                std::size_t used = 0;
                const long long v = std::stoll(token, &used);
                if (used == token.size()) keys_.push_back(static_cast<Key>(v));
            } catch (const std::exception&) {
                // not a number (e.g. a CSV header) -> skip
            }
        }
    }
    if (keys_.empty()) throw std::runtime_error("trace file contains no keys: " + path);
}

std::size_t TraceFileWorkload::resolveLength(std::size_t n) const {
    return n == 0 ? keys_.size() : n;
}

Key TraceFileWorkload::nextKey(std::size_t i, std::size_t) { return keys_[i % keys_.size()]; }

// ---------------------------------------------------------- WorkloadFactory
std::unique_ptr<WorkloadGenerator> WorkloadFactory::create(const std::string& rawName,
                                                           const WorkloadParams& p) {
    std::string name = rawName;
    std::transform(name.begin(), name.end(), name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    const std::size_t cap = atLeastOne(p.capacity);
    if (name == "sequential") return std::make_unique<SequentialWorkload>(p.universe, 16, p.seed);
    if (name == "uniform") return std::make_unique<UniformRandomWorkload>(p.universe / 2, p.seed);
    if (name == "zipf") return std::make_unique<ZipfWorkload>(p.universe, p.alpha, p.seed);
    // Loop 20% larger than the cache: the textbook LRU/FIFO worst case.
    if (name == "looping") return std::make_unique<LoopingWorkload>(cap + cap / 5 + 1, p.seed);
    // Hot set = 80% of the cache; every 500 hot requests, scan 2x cache of new keys.
    if (name == "scan")
        return std::make_unique<ScanResistanceWorkload>(cap * 4 / 5 + 1, 500, cap * 2, p.seed);
    if (name == "phase")
        return std::make_unique<PhaseShiftWorkload>(p.universe, cap * 4 / 5 + 1, p.phases, 0.9,
                                                    p.seed);
    if (name == "trace") return std::make_unique<TraceFileWorkload>(p.traceFile);
    throw std::invalid_argument("unknown workload '" + rawName +
                                "' (try: sequential, uniform, zipf, looping, scan, phase, trace)");
}

std::vector<std::string> WorkloadFactory::available() {
    return {"sequential", "uniform", "zipf", "looping", "scan", "phase"};
}

}  // namespace cachesim
