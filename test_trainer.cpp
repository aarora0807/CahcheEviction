// test_trainer.cpp - genetic algorithm + weight persistence.
#include <cstdio>
#include <fstream>

#include "PolicyTrainer.h"
#include "Workloads.h"
#include "doctest.h"

using namespace cachesim;

namespace {
std::vector<TrainingCase> smallCases() {
    ScanResistanceWorkload scan(16, 200, 40, 1);
    ZipfWorkload zipf(400, 0.9, 2);
    LoopingWorkload loop(25, 3);
    return {{"scan", scan.generate(3000), 20},
            {"zipf", zipf.generate(3000), 20},
            {"loop", loop.generate(3000), 20}};
}
TrainerConfig smallConfig(std::size_t threads) {
    TrainerConfig c;
    c.populationSize = 12;
    c.generations = 6;
    c.threads = threads;
    c.seed = 99;
    return c;
}
}  // namespace

TEST_CASE("Weights: save/load round trip and malformed files") {
    const char* path = "weights_tmp_test.txt";
    const AIWeights w({0.25, -0.5, 0.125, 1.0});
    w.save(path);
    CHECK(AIWeights::load(path).values == w.values);
    {
        std::ofstream bad(path);
        bad << "# only two numbers\n1 2\n";
    }
    CHECK_THROWS_AS(AIWeights::load(path), std::runtime_error);
    std::remove(path);
    CHECK_THROWS_AS(AIWeights::load("missing_weights_file.txt"), std::runtime_error);
}

TEST_CASE("Trainer: history, elitism and improvement over generation 0") {
    PolicyTrainer t(smallConfig(2), smallCases());
    const AIWeights best = t.train();
    const auto& h = t.history();
    REQUIRE(h.size() == 6);
    for (const auto& g : h) {
        CHECK(g.best >= g.mean);
        CHECK(g.mean >= g.worst);
    }
    // Elitism => the best fitness never decreases.
    for (std::size_t i = 1; i < h.size(); ++i) CHECK(h[i].best >= h[i - 1].best - 1e-12);
    // Returned genome really scores the reported best fitness.
    CHECK(t.fitness(best) == doctest::Approx(h.back().best));
    // Seeded heuristics => never worse than plain LRU on the training set.
    CHECK(t.fitness(best) >= t.fitness(AIWeights({1.0, 0.0, 0.0, 0.0})) - 1e-12);
    for (double w : best.values) {
        CHECK(w >= -1.0);
        CHECK(w <= 1.0);
    }
}

TEST_CASE("Trainer: deterministic regardless of thread count") {
    PolicyTrainer one(smallConfig(1), smallCases());
    PolicyTrainer four(smallConfig(4), smallCases());
    CHECK(one.train().values == four.train().values);
}

TEST_CASE("Trainer: invalid configurations are rejected") {
    TrainerConfig c = smallConfig(1);
    CHECK_THROWS_AS(PolicyTrainer(c, {}), std::invalid_argument);
    c.populationSize = 2;
    CHECK_THROWS_AS(PolicyTrainer(c, smallCases()), std::invalid_argument);
}
