// AIPolicy.h - feature-scored eviction policy whose weights are learned by a GA.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "EvictionPolicy.h"

namespace cachesim {

/// The chromosome the PolicyTrainer evolves: one weight per feature.
/// Feature order: [0] recency, [1] frequency, [2] age, [3] inter-arrival gap.
struct AIWeights {
    static constexpr std::size_t kNumFeatures = 4;
    std::vector<double> values;

    AIWeights();  // = defaults()
    explicit AIWeights(std::vector<double> v);  // throws if size != kNumFeatures

    /// Hand-picked starting point: mostly recency, a little anti-frequency.
    static AIWeights defaults();
    /// Persist / restore weights so training and evaluation are separate runs.
    void save(const std::string& path) const;           // throws std::runtime_error
    static AIWeights load(const std::string& path);      // throws std::runtime_error
    std::string toString() const;
    static const std::array<const char*, kNumFeatures>& featureNames();
};

/// [The "AI" policy]
/// Each resident key is described by four features normalised to [0,1]:
///   f1 recency   = (now - lastAccess) / max over candidates
///   f2 frequency = log(1+count) / log(1+maxCount)
///   f3 age       = (now - insertTime) / max over candidates
///   f4 gap       = mean inter-arrival time ((now-insert)/count) / max
/// score = sum_i w_i * f_i ; the key with the HIGHEST score is evicted.
/// Normalising by the candidate maximum makes the score independent of cache
/// size and trace length, so weights learned at one size transfer to another.
///
/// Victim search:
///   sampleK == 0 : full scan, O(n) per eviction, exact argmax.
///   sampleK  > 0 : score K random residents (Redis-style), O(K), approximate.
class AIPolicy final : public EvictionPolicy {
public:
    explicit AIPolicy(AIWeights weights = AIWeights::defaults(), std::size_t sampleK = 0,
                      std::uint64_t seed = 7);

    void onInsert(Key key) override;
    void onAccess(Key key) override;
    void onRemove(Key key) override;
    Key selectVictim() override;
    std::string name() const override;
    std::size_t trackedCount() const override { return entries_.size(); }

    const AIWeights& weights() const { return weights_; }
    /// Normalised feature vector of a resident key, using all residents as the
    /// normalisation set (exposed for tests and for explaining decisions).
    std::array<double, AIWeights::kNumFeatures> featuresOf(Key key) const;

private:
    struct Entry {
        Key key;
        std::uint64_t insertTime;
        std::uint64_t lastAccess;
        std::uint64_t count;
    };
    struct Raw {  // un-normalised features of one entry
        double recency, logCount, age, gap;
    };
    Raw rawFeatures(const Entry& e) const;

    AIWeights weights_;
    std::size_t sampleK_;
    std::mt19937_64 rng_;
    std::uint64_t clock_ = 0;                    // logical time: one tick per request
    std::vector<Entry> entries_;                 // dense array -> cache-friendly scans
    std::unordered_map<Key, std::size_t> index_;  // key -> position in entries_
    std::vector<std::size_t> candidates_;        // scratch buffer, reused per eviction
};

}  // namespace cachesim
