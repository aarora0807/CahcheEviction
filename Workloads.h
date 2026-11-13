// Workloads.h - concrete request patterns, each stressing a different policy.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Workload.h"

namespace cachesim {

/// Short sequential runs (e.g. file reads) starting at random offsets.
class SequentialWorkload final : public WorkloadGenerator {
public:
    SequentialWorkload(std::size_t universe, std::size_t runLength, std::uint64_t seed);
    std::string name() const override { return "sequential"; }

protected:
    void prepare(std::size_t n) override;
    Key nextKey(std::size_t i, std::size_t n) override;

private:
    std::size_t universe_, runLength_;
    Key runStart_ = 0;
};

/// Every key in [0, universe) equally likely: no locality at all.
class UniformRandomWorkload final : public WorkloadGenerator {
public:
    UniformRandomWorkload(std::size_t universe, std::uint64_t seed);
    std::string name() const override { return "uniform"; }

protected:
    Key nextKey(std::size_t i, std::size_t n) override;

private:
    std::size_t universe_;
};

/// P(rank k) ~ 1 / k^alpha. alpha ~0.6-1.2 matches web/CDN popularity.
class ZipfWorkload final : public WorkloadGenerator {
public:
    ZipfWorkload(std::size_t universe, double alpha, std::uint64_t seed);
    std::string name() const override;

protected:
    Key nextKey(std::size_t i, std::size_t n) override;

private:
    double alpha_;
    std::vector<double> cdf_;  // precomputed once; sampling is a binary search
};

/// 0,1,...,L-1,0,1,... with L slightly larger than the cache: LRU/FIFO get 0%.
class LoopingWorkload final : public WorkloadGenerator {
public:
    LoopingWorkload(std::size_t loopLength, std::uint64_t seed);
    std::string name() const override { return "looping"; }

protected:
    Key nextKey(std::size_t i, std::size_t n) override;

private:
    std::size_t loopLength_;
};

/// A small hot set accessed repeatedly, interrupted by one-time scans of
/// never-reused keys. Scans flush LRU; frequency-aware policies resist them.
class ScanResistanceWorkload final : public WorkloadGenerator {
public:
    ScanResistanceWorkload(std::size_t hotSetSize, std::size_t hotBurst, std::size_t scanLength,
                           std::uint64_t seed);
    std::string name() const override { return "scan"; }

protected:
    void prepare(std::size_t n) override;
    Key nextKey(std::size_t i, std::size_t n) override;

private:
    std::size_t hotSetSize_, hotBurst_, scanLength_;
    Key nextScanKey_ = 0;
};

/// The hot set moves to new keys `phases` times: tests adaptivity
/// (LFU's stale high counts make it slow to forget the old hot set).
class PhaseShiftWorkload final : public WorkloadGenerator {
public:
    PhaseShiftWorkload(std::size_t universe, std::size_t hotSetSize, std::size_t phases,
                       double hotProbability, std::uint64_t seed);
    std::string name() const override { return "phase"; }

protected:
    Key nextKey(std::size_t i, std::size_t n) override;

private:
    std::size_t universe_, hotSetSize_, phases_;
    double hotProbability_;
};

/// Replays keys from a text/CSV file (whitespace or comma separated, '#'
/// comments, non-numeric tokens such as a CSV header are skipped).
class TraceFileWorkload final : public WorkloadGenerator {
public:
    explicit TraceFileWorkload(const std::string& path);  // throws on I/O error / empty
    std::string name() const override { return "trace"; }
    std::size_t length() const { return keys_.size(); }

protected:
    std::size_t resolveLength(std::size_t n) const override;
    Key nextKey(std::size_t i, std::size_t n) override;  // cycles if n > length

private:
    std::vector<Key> keys_;
};

/// Parameters for building any workload by name.
struct WorkloadParams {
    std::size_t capacity = 50;   // workloads sized relative to the cache
    std::size_t universe = 1000;
    double alpha = 0.9;
    std::size_t phases = 2;
    std::uint64_t seed = 42;
    std::string traceFile = "data/sample_trace.txt";
};

/// [Design pattern: FACTORY] for workloads.
class WorkloadFactory {
public:
    static std::unique_ptr<WorkloadGenerator> create(const std::string& name,
                                                     const WorkloadParams& params);
    /// Names accepted by create() (the synthetic ones; "trace" needs a file).
    static std::vector<std::string> available();
};

}  // namespace cachesim
