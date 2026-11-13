// WebExporter.cpp
#include "WebExporter.h"

#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>

#include "PolicyFactory.h"

namespace cachesim {

namespace {

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') out += '\\';
        if (c == '\n') { out += "\\n"; continue; }
        out += c;
    }
    return out + "\"";
}

std::string num(double x) {
    std::ostringstream o;
    o << std::setprecision(6) << x;
    return o.str();
}

std::string keyList(const std::vector<Key>& keys) {
    std::string out = "[";
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (i) out += ',';
        out += std::to_string(keys[i]);
    }
    return out + "]";
}

std::string join(const std::vector<std::string>& parts) {
    std::string out = "[";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) out += ",\n  ";
        out += parts[i];
    }
    return out + "]";
}

std::vector<std::vector<std::string>> readCsv(const std::string& path) {
    std::vector<std::vector<std::string>> rows;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::vector<std::string> cells;
        std::stringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, ',')) cells.push_back(cell);
        rows.push_back(cells);
    }
    return rows;
}

/// [Observer] Records every request of a small run, for the animation.
class StepRecorder final : public CacheListener {
public:
    explicit StepRecorder(const AIWeights& w) : weights_(w) {}

    void onHit(Key) override { hit_ = true; }
    void onMiss(Key) override { hit_ = false; }
    void onEvict(Key victim, std::int64_t) override {
        victim_ = victim;
        hasVictim_ = true;
    }
    void onBeforeEvict(const Cache& cache) override {
        // Only the AI policy has scores worth showing.
        const auto* ai = dynamic_cast<const AIPolicy*>(&cache.policy());
        if (ai == nullptr) return;
        std::ostringstream o;
        o << '[';
        bool first = true;
        for (Key k : cache.keys()) {
            const auto f = ai->featuresOf(k);
            double score = 0.0;
            for (std::size_t i = 0; i < f.size(); ++i) score += weights_.values[i] * f[i];
            o << (first ? "" : ",") << "{\"k\":" << k << ",\"f\":[" << num(f[0]) << ','
              << num(f[1]) << ',' << num(f[2]) << ',' << num(f[3]) << "],\"s\":" << num(score)
              << '}';
            first = false;
        }
        o << ']';
        aiScores_ = o.str();
    }
    void onRequestComplete(const Cache& cache, Key key) override {
        std::string s = "{\"k\":" + std::to_string(key) + ",\"h\":" + (hit_ ? "1" : "0");
        if (hasVictim_) s += ",\"v\":" + std::to_string(victim_);
        s += ",\"c\":" + keyList(cache.keys());
        if (!aiScores_.empty()) s += ",\"ai\":" + aiScores_;
        steps.push_back(s + "}");
        hasVictim_ = false;
        aiScores_.clear();
    }

    std::vector<std::string> steps;

private:
    const AIWeights& weights_;
    bool hit_ = false;
    bool hasVictim_ = false;
    Key victim_ = 0;
    std::string aiScores_;
};

}  // namespace

WebExporter::WebExporter(AIWeights weights) : weights_(std::move(weights)) {}

void WebExporter::setResults(const std::vector<SimulationResult>& results, std::size_t capacity,
                             std::size_t requests, std::uint64_t seed) {
    settings_ = "{\"capacity\":" + std::to_string(capacity) +
                ",\"requests\":" + std::to_string(requests) + ",\"seed\":" + std::to_string(seed) +
                "}";
    results_.clear();
    for (const auto& r : results) {
        const Metrics& m = r.metrics;
        results_.push_back("{\"w\":" + quote(r.workload) + ",\"p\":" + quote(r.policy) +
                           ",\"hits\":" + std::to_string(m.hits) +
                           ",\"misses\":" + std::to_string(m.misses) +
                           ",\"ev\":" + std::to_string(m.evictions) +
                           ",\"hr\":" + num(m.hitRatio()) + ",\"ns\":" + num(m.nsPerRequest()) +
                           "}");
    }
}

void WebExporter::addScenario(const WebScenario& sc) {
    PolicyConfig cfg;
    cfg.capacity = sc.capacity;
    cfg.futureTrace = &sc.requests;
    cfg.aiWeights = weights_;
    const Simulator sim(sc.capacity);

    std::string runs = "{";
    bool first = true;
    for (const std::string name : {"fifo", "lru", "lfu", "ai", "adaptive", "belady"}) {
        auto policy = PolicyFactory::create(name, cfg);
        const std::string label = policy->name();
        StepRecorder rec(weights_);
        const Metrics m = sim.run(sc.requests, std::move(policy), &rec, false);
        runs += std::string(first ? "" : ",\n    ") + quote(label) + ":{\"hits\":" +
                std::to_string(m.hits) + ",\"misses\":" + std::to_string(m.misses) +
                ",\"steps\":" + join(rec.steps) + "}";
        first = false;
    }
    runs += "}";
    scenarios_.push_back("{\"id\":" + quote(sc.id) + ",\"title\":" + quote(sc.title) +
                         ",\"description\":" + quote(sc.description) +
                         ",\"capacity\":" + std::to_string(sc.capacity) +
                         ",\"requests\":" + keyList(sc.requests) + ",\"runs\":" + runs + "}");
}

void WebExporter::addWorkloadTrace(const std::string& name, const std::vector<Key>& trace,
                                   std::size_t capacity) {
    PolicyConfig cfg;
    cfg.capacity = capacity;
    cfg.futureTrace = &trace;
    cfg.aiWeights = weights_;
    const Simulator sim(capacity);
    std::string hits = "{";
    bool first = true;
    for (const std::string policy : {"fifo", "lru", "lfu", "ai", "adaptive", "belady"}) {
        auto p = PolicyFactory::create(policy, cfg);
        const std::string label = p->name();
        const Metrics m = sim.run(trace, std::move(p), nullptr, false);
        hits += std::string(first ? "" : ",") + quote(label) + ":" + std::to_string(m.hits);
        first = false;
    }
    traces_.push_back("{\"name\":" + quote(name) + ",\"capacity\":" + std::to_string(capacity) +
                      ",\"hits\":" + hits + "},\"keys\":" + keyList(trace) + "}");
}

void WebExporter::loadResultCsvs(const std::string& dir) {
    const std::string base = dir.empty() ? "" : dir + "/";
    const auto curve = readCsv(base + "training_curve.csv");
    if (curve.size() > 1) {
        std::string g, b, m, w;
        for (std::size_t i = 1; i < curve.size(); ++i) {
            if (curve[i].size() < 4) continue;
            const char* sep = (i > 1) ? "," : "";
            g += sep + curve[i][0];
            b += sep + curve[i][1];
            m += sep + curve[i][2];
            w += sep + curve[i][3];
        }
        training_ = "{\"gen\":[" + g + "],\"best\":[" + b + "],\"mean\":[" + m + "],\"worst\":[" +
                    w + "]}";
    }
    for (const auto& row : readCsv(base + "generalization.csv")) {
        if (row.size() < 3 || row[0] == "policy") continue;
        generalization_.push_back("{\"p\":" + quote(row[0]) + ",\"train\":" + row[1] +
                                  ",\"test\":" + row[2] + "}");
    }
    for (const auto& row : readCsv(base + "benchmark.csv")) {
        if (row.size() < 6 || row[0] == "policy") continue;
        benchmark_.push_back("{\"p\":" + quote(row[0]) + ",\"cap\":" + row[1] +
                             ",\"rps\":" + row[4] + ",\"hr\":" + row[5] + "}");
    }
}

void WebExporter::write(const std::string& path) const {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write " + path + " (does the folder exist?)");
    out << "// Generated by: cache_sim --export-web   (do not edit by hand)\n";
    out << "window.SIM_DATA = {\n\"version\": 1,\n\"settings\": " << settings_ << ",\n";
    out << "\"weights\": [";
    for (std::size_t i = 0; i < weights_.values.size(); ++i)
        out << (i ? "," : "") << num(weights_.values[i]);
    out << "],\n\"featureNames\": [\"recency\",\"frequency\",\"age\",\"gap\"],\n";
    out << "\"results\": " << join(results_) << ",\n";
    out << "\"scenarios\": " << join(scenarios_) << ",\n";
    out << "\"traces\": " << join(traces_) << ",\n";
    out << "\"training\": " << training_ << ",\n";
    out << "\"generalization\": " << join(generalization_) << ",\n";
    out << "\"benchmark\": " << join(benchmark_) << "\n};\n";
    if (!out) throw std::runtime_error("error while writing " + path);
}

}  // namespace cachesim
