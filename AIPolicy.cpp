// AIPolicy.cpp
#include "AIPolicy.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace cachesim {

// ---------------------------------------------------------------- AIWeights
AIWeights::AIWeights() : values(defaults().values) {}

AIWeights::AIWeights(std::vector<double> v) : values(std::move(v)) {
    if (values.size() != kNumFeatures) {
        throw std::invalid_argument("AIWeights: expected " + std::to_string(kNumFeatures) +
                                    " weights, got " + std::to_string(values.size()));
    }
}

AIWeights AIWeights::defaults() {
    // Mostly LRU, nudged towards keeping frequently-used keys.
    return AIWeights(std::vector<double>{1.0, -0.5, 0.0, 0.0});
}

const std::array<const char*, AIWeights::kNumFeatures>& AIWeights::featureNames() {
    static const std::array<const char*, kNumFeatures> names{"recency", "frequency", "age",
                                                             "gap"};
    return names;
}

void AIWeights::save(const std::string& path) const {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write weights file: " + path);
    out << "# AIPolicy weights (evolved by PolicyTrainer)\n";
    out << "# order: recency frequency age gap ; evict argmax(sum w_i * f_i)\n";
    out << std::setprecision(17);
    for (std::size_t i = 0; i < values.size(); ++i) out << (i ? " " : "") << values[i];
    out << '\n';
}

AIWeights AIWeights::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open weights file: " + path);
    std::vector<double> v;
    std::string line;
    while (std::getline(in, line) && v.size() < kNumFeatures) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        double x = 0.0;
        while (v.size() < kNumFeatures && ss >> x) v.push_back(x);
    }
    if (v.size() != kNumFeatures) throw std::runtime_error("malformed weights file: " + path);
    return AIWeights(std::move(v));
}

std::string AIWeights::toString() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    for (std::size_t i = 0; i < values.size(); ++i) {
        ss << (i ? ", " : "") << featureNames()[i] << '=' << values[i];
    }
    return ss.str();
}

// ----------------------------------------------------------------- AIPolicy
AIPolicy::AIPolicy(AIWeights weights, std::size_t sampleK, std::uint64_t seed)
    : weights_(std::move(weights)), sampleK_(sampleK), rng_(seed) {}

std::string AIPolicy::name() const {
    return sampleK_ == 0 ? "AI" : "AI-K" + std::to_string(sampleK_);
}

void AIPolicy::onInsert(Key key) {
    ++clock_;
    if (index_.count(key) != 0) {  // defensive: treat as access
        Entry& e = entries_[index_[key]];
        e.lastAccess = clock_;
        ++e.count;
        return;
    }
    index_[key] = entries_.size();
    entries_.push_back(Entry{key, clock_, clock_, 1});
}

void AIPolicy::onAccess(Key key) {
    ++clock_;
    auto it = index_.find(key);
    if (it == index_.end()) return;
    Entry& e = entries_[it->second];
    e.lastAccess = clock_;
    ++e.count;
}

void AIPolicy::onRemove(Key key) {
    auto it = index_.find(key);
    if (it == index_.end()) return;
    // O(1) "swap and pop" keeps entries_ dense.
    const std::size_t pos = it->second;
    const std::size_t last = entries_.size() - 1;
    if (pos != last) {
        entries_[pos] = entries_[last];
        index_[entries_[pos].key] = pos;
    }
    entries_.pop_back();
    index_.erase(it);
}

AIPolicy::Raw AIPolicy::rawFeatures(const Entry& e) const {
    const double now = static_cast<double>(clock_) + 1.0;  // time of the incoming request
    const double age = now - static_cast<double>(e.insertTime);
    return Raw{now - static_cast<double>(e.lastAccess),
               std::log1p(static_cast<double>(e.count)), age,
               age / static_cast<double>(e.count)};
}

Key AIPolicy::selectVictim() {
    if (entries_.empty()) throw std::logic_error("AIPolicy::selectVictim on empty policy");

    // 1. Choose candidates: everybody (exact) or K random residents (approximate).
    candidates_.clear();
    if (sampleK_ == 0 || sampleK_ >= entries_.size()) {
        for (std::size_t i = 0; i < entries_.size(); ++i) candidates_.push_back(i);
    } else {
        std::uniform_int_distribution<std::size_t> pick(0, entries_.size() - 1);
        for (std::size_t i = 0; i < sampleK_; ++i) candidates_.push_back(pick(rng_));
    }

    // 2. Normalisers = maxima over the candidate set.
    Raw maxRaw{0.0, 0.0, 0.0, 0.0};
    for (std::size_t idx : candidates_) {
        const Raw r = rawFeatures(entries_[idx]);
        maxRaw.recency = std::max(maxRaw.recency, r.recency);
        maxRaw.logCount = std::max(maxRaw.logCount, r.logCount);
        maxRaw.age = std::max(maxRaw.age, r.age);
        maxRaw.gap = std::max(maxRaw.gap, r.gap);
    }
    auto norm = [](double x, double m) { return m > 0.0 ? x / m : 0.0; };
    const std::vector<double>& w = weights_.values;

    // 3. Linear score, evict the arg-max. Ties keep the first candidate (deterministic).
    double bestScore = -std::numeric_limits<double>::infinity();
    std::size_t bestIdx = candidates_.front();
    for (std::size_t idx : candidates_) {
        const Raw r = rawFeatures(entries_[idx]);
        const double score = w[0] * norm(r.recency, maxRaw.recency) +
                             w[1] * norm(r.logCount, maxRaw.logCount) +
                             w[2] * norm(r.age, maxRaw.age) + w[3] * norm(r.gap, maxRaw.gap);
        if (score > bestScore) {
            bestScore = score;
            bestIdx = idx;
        }
    }
    return entries_[bestIdx].key;
}

std::array<double, AIWeights::kNumFeatures> AIPolicy::featuresOf(Key key) const {
    std::array<double, AIWeights::kNumFeatures> f{0.0, 0.0, 0.0, 0.0};
    auto it = index_.find(key);
    if (it == index_.end()) return f;
    Raw maxRaw{0.0, 0.0, 0.0, 0.0};
    for (const Entry& e : entries_) {
        const Raw r = rawFeatures(e);
        maxRaw.recency = std::max(maxRaw.recency, r.recency);
        maxRaw.logCount = std::max(maxRaw.logCount, r.logCount);
        maxRaw.age = std::max(maxRaw.age, r.age);
        maxRaw.gap = std::max(maxRaw.gap, r.gap);
    }
    const Raw r = rawFeatures(entries_[it->second]);
    auto norm = [](double x, double m) { return m > 0.0 ? x / m : 0.0; };
    f[0] = norm(r.recency, maxRaw.recency);
    f[1] = norm(r.logCount, maxRaw.logCount);
    f[2] = norm(r.age, maxRaw.age);
    f[3] = norm(r.gap, maxRaw.gap);
    return f;
}

}  // namespace cachesim
