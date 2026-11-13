// test_workloads.cpp - generators are reproducible and have the intended shape.
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>

#include "Workloads.h"
#include "doctest.h"

using namespace cachesim;

TEST_CASE("Workloads: reproducible, correct length, every factory name works") {
    WorkloadParams p;
    for (const auto& name : WorkloadFactory::available()) {
        auto g = WorkloadFactory::create(name, p);
        const auto a = g->generate(1000);
        const auto b = g->generate(1000);
        CAPTURE(name);
        CHECK(a.size() == 1000);
        CHECK(a == b);  // same seed -> identical trace
        CHECK(g->generate(0).empty());
    }
    CHECK_THROWS_AS(WorkloadFactory::create("nope", p), std::invalid_argument);
}

TEST_CASE("Workloads: different seeds give different traces") {
    UniformRandomWorkload a(1000, 1), b(1000, 2);
    CHECK(a.generate(500) != b.generate(500));
}

TEST_CASE("Uniform: keys stay inside the universe") {
    UniformRandomWorkload u(50, 3);
    for (Key k : u.generate(5000)) {
        CHECK(k >= 0);
        CHECK(k < 50);
    }
}

TEST_CASE("Zipf: rank 0 is the most popular key and skew grows with alpha") {
    auto share0 = [](double alpha) {
        ZipfWorkload z(1000, alpha, 9);
        const auto t = z.generate(50000);
        std::map<Key, int> freq;
        for (Key k : t) ++freq[k];
        const auto top = std::max_element(freq.begin(), freq.end(),
                                          [](auto& x, auto& y) { return x.second < y.second; });
        CHECK(top->first == 0);
        return static_cast<double>(freq[0]) / static_cast<double>(t.size());
    };
    CHECK(share0(1.2) > share0(0.6));
}

TEST_CASE("Looping: exact cyclic pattern") {
    LoopingWorkload l(4, 1);
    CHECK(l.generate(10) == std::vector<Key>{0, 1, 2, 3, 0, 1, 2, 3, 0, 1});
}

TEST_CASE("Sequential: runs of consecutive keys") {
    SequentialWorkload s(1000, 8, 4);
    const auto t = s.generate(64);
    for (std::size_t i = 0; i < t.size(); ++i) {
        if (i % 8 != 0) CHECK(t[i] == (t[i - 1] + 1) % 1000);
    }
}

TEST_CASE("Scan: scan keys are used exactly once, hot keys repeat") {
    ScanResistanceWorkload s(10, 100, 50, 2);
    const auto t = s.generate(1500);
    std::map<Key, int> freq;
    for (Key k : t) ++freq[k];
    for (const auto& kv : freq) {
        if (kv.first >= 1000000) CHECK(kv.second == 1);
        else CHECK(kv.first < 10);
    }
}

TEST_CASE("Phase: the hot set moves between halves") {
    PhaseShiftWorkload p(1000, 20, 2, 1.0, 5);  // 100% hot requests
    const auto t = p.generate(2000);
    std::set<Key> first(t.begin(), t.begin() + 1000), second(t.begin() + 1000, t.end());
    CHECK(*first.rbegin() < 20);
    CHECK(*second.begin() >= 20);
}

TEST_CASE("TraceFile: parses numbers, commas, comments and skips headers") {
    const char* path = "test_trace_tmp.csv";
    {
        std::ofstream out(path);
        out << "key\n# comment line\n1, 2,3\n4 5 # trailing comment\nabc 6\n";
    }
    TraceFileWorkload t(path);
    CHECK(t.length() == 6);
    CHECK(t.generate(0) == std::vector<Key>{1, 2, 3, 4, 5, 6});
    CHECK(t.generate(8) == std::vector<Key>{1, 2, 3, 4, 5, 6, 1, 2});  // cycles
    std::remove(path);
    CHECK_THROWS_AS(TraceFileWorkload("does/not/exist.txt"), std::runtime_error);
}
