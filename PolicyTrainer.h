// PolicyTrainer.h - genetic algorithm that evolves AIPolicy weights.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "AIPolicy.h"
#include "Types.h"

namespace cachesim {

/// A trace + cache size on which a candidate weight vector is scored.
struct TrainingCase {
    std::string name;
    std::vector<Key> trace;
    std::size_t capacity = 50;
};

struct TrainerConfig {
    std::size_t populationSize = 40;
    std::size_t generations = 60;
    std::size_t tournamentK = 3;
    std::size_t eliteCount = 2;
    double crossoverRate = 0.9;
    double blendAlpha = 0.25;     // BLX-alpha: children may land slightly outside parents
    double mutationRate = 0.1;    // per-gene probability
    double sigmaStart = 0.30;     // Gaussian mutation std-dev, decays to sigmaEnd
    double sigmaEnd = 0.02;
    double timePenalty = 0.0;     // fitness -= penalty * (avg decision ns / 1000)
    std::size_t sampleK = 0;      // victim search used while training (0 = full scan)
    std::size_t threads = 0;      // 0 = std::thread::hardware_concurrency()
    bool seedHeuristics = true;   // put LRU/LFU/FIFO-equivalent genomes in gen 0
    std::uint64_t seed = 2024;
};

struct GenerationStats {
    std::size_t generation = 0;
    double best = 0.0, mean = 0.0, worst = 0.0;
    double sigma = 0.0;
    std::vector<double> bestWeights;
};

/// Evolutionary search over w in [-1,1]^4.
///   fitness    = mean hit ratio over ALL training cases (avoids overfitting one)
///   selection  = tournament (k = 3) + elitism (top 2 copied unchanged)
///   crossover  = blend (BLX-alpha) arithmetic crossover
///   mutation   = Gaussian, per-gene rate 0.1, sigma decays exponentially
/// Fitness evaluations are independent, so they run in parallel with
/// std::async; each task builds its own Simulator + policy and only reads the
/// shared (const) traces -> no locks, no data races, deterministic results.
class PolicyTrainer {
public:
    PolicyTrainer(TrainerConfig config, std::vector<TrainingCase> cases);

    /// Runs the GA; `onGeneration` (optional) is called after each generation.
    AIWeights train(const std::function<void(const GenerationStats&)>& onGeneration = nullptr);

    /// Fitness of one weight vector on this trainer's cases.
    double fitness(const AIWeights& weights) const;
    /// Mean hit ratio of `weights` on any case set (used for train/test reports).
    static double meanHitRatio(const AIWeights& weights, const std::vector<TrainingCase>& cases,
                               std::size_t sampleK = 0);

    const std::vector<GenerationStats>& history() const { return history_; }
    const TrainerConfig& config() const { return config_; }

private:
    using Chromosome = std::vector<double>;

    std::vector<double> evaluatePopulation(const std::vector<Chromosome>& population) const;
    const Chromosome& tournament(const std::vector<Chromosome>& population,
                                 const std::vector<double>& fitness, std::mt19937_64& rng) const;
    Chromosome crossover(const Chromosome& a, const Chromosome& b, std::mt19937_64& rng) const;
    void mutate(Chromosome& c, double sigma, std::mt19937_64& rng) const;

    TrainerConfig config_;
    std::vector<TrainingCase> cases_;
    std::vector<GenerationStats> history_;
};

}  // namespace cachesim
