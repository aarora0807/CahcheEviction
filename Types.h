// Types.h - shared vocabulary types for the simulator.
#pragma once

#include <cstdint>

namespace cachesim {

/// Cache keys are 64-bit integers: traces from real systems (block numbers,
/// object ids, hashed URLs) fit naturally, and hashing an int is cheap.
using Key = std::int64_t;

/// Values are irrelevant to hit/miss behaviour; we store a 64-bit payload so
/// the Cache still behaves like a real key/value store.
using Value = std::int64_t;

}  // namespace cachesim
