// PolicyTrainer.cpp - the genetic algorithm.
#include "PolicyTrainer.h"

#include <algorithm>
#include <cmath>
#include <future>
#include <numeric>
#include <stdexcept>
#include <thread>

#include "Simulator.h"

namespace cachesim {

PolicyTrainer::PolicyTrainer(TrainerConfig config, std::vector<TrainingCase> cases)
    : config_(config), cases_(std::move(cases)) {
    if (cases_.empty()) throw std::invalid_argument("PolicyTrainer needs at least one case");
    if (config_.populationSize < 4) throw std::invalid_argument("population must be >= 4");
    if (config_.generations == 0) throw std::invalid_argument("generations must be >= 1");
    if (config_.eliteCount >= config_.populationSize) {
        throw std::invalid_argument("eliteCount must be smaller than the population");
    }
    if (config_.tournamentK == 0) config_.tournamentK = 1;
}

double PolicyTrainer::meanHitRatio(const AIWeights& weights,
                                   const std::vector<TrainingCase>& cases, std::size_t sampleK) {
    if (cases.empty()) return 0.0;
    double sum = 0.0;
    for (const TrainingCase& c : cases) {
        const Simulator sim(c.capacity);
        sum += sim.run(c.trace, std::make_unique<AIPolicy>(weights, sampleK), nullptr, false)
                   .hitRatio();
    }
    return sum / static_cast<double>(cases.size());
}

double PolicyTrainer::fitness(const AIWeights& weights) const {
    if (config_.timePenalty <= 0.0) return meanHitRatio(weights, cases_, config_.sampleK);
    // Optional speed-aware fitness (makes results timing-dependent, so off by default).
    double sum = 0.0;
    for (const TrainingCase& c : cases_) {
        const Simulator sim(c.capacity);
        const Metrics m = sim.run(c.trace, std::make_unique<AIPolicy>(weights, config_.sampleK));
        sum += m.hitRatio() - config_.timePenalty * m.avgDecisionNs() / 1000.0;
    }
    return sum / static_cast<double>(cases_.size());
}

std::vector<double> PolicyTrainer::evaluatePopulation(
    const std::vector<Chromosome>& population) const {
    std::vector<double> scores(population.size(), 0.0);
    std::size_t threads = config_.threads;
    if (threads == 0) threads = std::max(1u, std::thread::hardware_concurrency());
    threads = std::min(threads, population.size());

    // Static partition: task t evaluates individuals t, t+T, t+2T, ...
    // Each task writes only its own slots of `scores` -> no synchronisation needed.
    std::vector<std::future<void>> tasks;
    for (std::size_t t = 0; t < threads; ++t) {
        tasks.push_back(std::async(std::launch::async, [&, t] {
            for (std::size_t i = t; i < population.size(); i += threads) {
                scores[i] = fitness(AIWeights(population[i]));
            }
        }));
    }
    for (auto& f : tasks) f.get();  // rethrows any exception from a worker
    return scores;
}

const PolicyTrainer::Chromosome& PolicyTrainer::tournament(
    const std::vector<Chromosome>& population, const std::vector<double>& fit,
    std::mt19937_64& rng) const {
    std::uniform_int_distribution<std::size_t> pick(0, population.size() - 1);
    std::size_t best = pick(rng);
    for (std::size_t i = 1; i < config_.tournamentK; ++i) {
        const std::size_t c = pick(rng);
        if (fit[c] > fit[best]) best = c;
    }
    return population[best];
}

PolicyTrainer::Chromosome PolicyTrainer::crossover(const Chromosome& a, const Chromosome& b,
                                                   std::mt19937_64& rng) const {
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    if (coin(rng) > config_.crossoverRate) return a;
    // BLX-alpha: child gene uniformly drawn from the parents' interval widened by alpha.
    std::uniform_real_distribution<double> mix(-config_.blendAlpha, 1.0 + config_.blendAlpha);
    Chromosome child(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double t = mix(rng);
        child[i] = std::clamp(a[i] + t * (b[i] - a[i]), -1.0, 1.0);
    }
    return child;
}

void PolicyTrainer::mutate(Chromosome& c, double sigma, std::mt19937_64& rng) const {
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    std::normal_distribution<double> noise(0.0, sigma);
    for (double& g : c) {
        if (coin(rng) < config_.mutationRate) g = std::clamp(g + noise(rng), -1.0, 1.0);
    }
}

AIWeights PolicyTrainer::train(const std::function<void(const GenerationStats&)>& onGeneration) {
    std::mt19937_64 rng(config_.seed);
    std::uniform_real_distribution<double> gene(-1.0, 1.0);
    const std::size_t nf = AIWeights::kNumFeatures;

    // Generation 0: random genomes (+ optional known heuristics as "seeds").
    std::vector<Chromosome> population;
    if (config_.seedHeuristics) {
        population.push_back({1.0, 0.0, 0.0, 0.0});    // == LRU
        population.push_back({0.0, 0.0, 1.0, 0.0});    // == FIFO
        population.push_back({0.02, -1.0, 0.0, 0.0});  // ~ LFU with LRU tie-break
    }
    while (population.size() < config_.populationSize) {
        Chromosome c(nf);
        for (double& g : c) g = gene(rng);
        population.push_back(c);
    }

    history_.clear();
    Chromosome bestEver = population.front();
    double bestEverFit = -1e300;

    for (std::size_t gen = 0; gen < config_.generations; ++gen) {
        const std::vector<double> fit = evaluatePopulation(population);

        std::vector<std::size_t> order(population.size());
        std::iota(order.begin(), order.end(), 0);
        std::sort(order.begin(), order.end(),
                  [&](std::size_t a, std::size_t b) { return fit[a] > fit[b]; });

        const double progress =
            config_.generations > 1 ? static_cast<double>(gen) / static_cast<double>(config_.generations - 1) : 1.0;
        const double sigma =
            config_.sigmaStart * std::pow(config_.sigmaEnd / config_.sigmaStart, progress);

        GenerationStats st;
        st.generation = gen;
        st.best = fit[order.front()];
        st.worst = fit[order.back()];
        st.mean = std::accumulate(fit.begin(), fit.end(), 0.0) / static_cast<double>(fit.size());
        st.sigma = sigma;
        st.bestWeights = population[order.front()];
        history_.push_back(st);
        if (st.best > bestEverFit) {
            bestEverFit = st.best;
            bestEver = population[order.front()];
        }
        if (onGeneration) onGeneration(st);
        if (gen + 1 == config_.generations) break;

        // Next generation: elites survive unchanged, the rest are bred.
        std::vector<Chromosome> next;
        next.reserve(population.size());
        for (std::size_t e = 0; e < config_.eliteCount; ++e) next.push_back(population[order[e]]);
        while (next.size() < population.size()) {
            const Chromosome& p1 = tournament(population, fit, rng);
            const Chromosome& p2 = tournament(population, fit, rng);
            Chromosome child = crossover(p1, p2, rng);
            mutate(child, sigma, rng);
            next.push_back(std::move(child));
        }
        population = std::move(next);
    }
    return AIWeights(bestEver);
}

}  // namespace cachesim
