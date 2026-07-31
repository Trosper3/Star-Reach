#pragma once

#include <cstdint>

// core/galaxy/Seeding -- the seed cascade behind features.md 7.1 (architecture.md 12.4).
//
// features.md 7.1 specifies a *schema* (parent seed + coordinates -> child seed), not an
// algorithm. Two contributors given only that sentence produce two incompatible galaxies from
// the same seed, so the algorithm is pinned here rather than left to whoever implements the next
// consumer:
//   - The mixer is SplitMix64 (Vigna 2015), constants below. Never std::hash -- it is
//     implementation-defined and libstdc++/libc++/MSVC disagree, which would desync a galaxy
//     across exactly the three platforms CI builds.
//   - Never std::shuffle / std::uniform_int_distribution for anything persisted -- those
//     algorithms are unspecified across standard libraries too. Everything here is hand-rolled
//     integer arithmetic instead.
//   - Coordinate packing (below) is part of the format. Changing the bit layout silently
//     regenerates every galaxy already generated under the old one.
//
// Pure functions, no state: every function takes its parent seed as a value and returns a new
// one. That is what makes generation order-independent (architecture.md 12.4 -- visiting system C
// first produces the same C as visiting A, B, then C) and what keeps sr_core linkable with no
// registry, no mode, and no window (Law 8).
//
// What this module does NOT do: turn a leaf seed into an actual star class, planet count, or
// deposit richness. That is generation content for whichever system consumes
// DeriveCelestialSeed's output, not part of the cascade itself. It also does not decide which
// state is seed-derived at all -- features.md 7.2's rule is that anything a player or faction
// could have changed (ownership, stations, depletion, discovery) is never regenerated from a
// seed; it lives in core/galaxy/ records instead.
namespace sr::core::galaxy {

// SplitMix64 (Vigna 2015) constants, written down rather than left implicit -- see this header's
// top comment on why "a hash function" is not a specification.
inline constexpr std::uint64_t kSplitMix64Gamma = 0x9E3779B97F4A7C15ULL;
inline constexpr std::uint64_t kSplitMix64Mix1 = 0xBF58476D1CE4E5B9ULL;
inline constexpr std::uint64_t kSplitMix64Mix2 = 0x94D049BB133111EBULL;

// One SplitMix64 step. Advances `state` in place and returns the mixed output word. The only
// randomness primitive this module uses -- every Derive*Seed function below is built from it.
std::uint64_t SplitMix64(std::uint64_t& state);

// A position at one cascade level: a galaxy's grid cell in the universe, a solar system's grid
// cell within its galaxy, or an orbit/belt slot within a solar system (which uses only `x` --
// `y` stays 0; see DeriveCelestialSeed below). Signed so positions can be placed around a
// galactic origin, matching how the galaxy/warp rendering already lays systems out.
//
// Packing format (fixed -- part of the save format, do not change without a migration): `x`
// occupies the high 32 bits of the mixer input, `y` the low 32 bits, each reinterpreted as its
// unsigned bit pattern so negative coordinates round-trip losslessly through the XOR/multiply
// mix below.
struct Coordinate {
    std::int32_t x = 0;
    std::int32_t y = 0;
};

// Derives the child seed for one cascade arrow (features.md 7.1): folds `coordinate` into
// `parentSeed` and draws twice from SplitMix64 so the result depends on all of the parent seed's
// bits and all of the coordinate's bits, not just whichever 64 happened to dominate a single
// mix. Named per the doc's table rather than exposed as one generic function so a call site reads
// as the cascade itself.
std::uint64_t DeriveGalaxySeed(std::uint64_t universeSeed, Coordinate galaxyCoordinate);
std::uint64_t DeriveSolarSystemSeed(std::uint64_t galaxySeed, Coordinate systemCoordinate);

// `orbitCoordinate.y` must be 0 -- an orbit/belt position is a single slot index, not a 2D grid
// cell (see Coordinate's comment above).
std::uint64_t DeriveCelestialSeed(std::uint64_t solarSystemSeed, Coordinate orbitCoordinate);

}  // namespace sr::core::galaxy
