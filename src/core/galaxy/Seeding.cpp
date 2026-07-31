#include "core/galaxy/Seeding.h"

namespace sr::core::galaxy {
namespace {

// Packs a Coordinate into one 64-bit mixer input per Seeding.h's pinned layout: x in the high
// 32 bits, y in the low 32 bits, both reinterpreted as unsigned so negative values survive the
// round trip.
std::uint64_t PackCoordinate(Coordinate coordinate) {
    const auto x = static_cast<std::uint64_t>(static_cast<std::uint32_t>(coordinate.x));
    const auto y = static_cast<std::uint64_t>(static_cast<std::uint32_t>(coordinate.y));
    return (x << 32) | y;
}

// Shared body behind every Derive*Seed function -- one algorithm, so the three cascade levels
// can never silently drift apart from each other.
std::uint64_t DeriveChildSeed(std::uint64_t parentSeed, Coordinate coordinate) {
    std::uint64_t state = parentSeed ^ PackCoordinate(coordinate);
    // First draw folds the coordinate fully into the mixer state; the second is the seed this
    // function returns. Two draws (rather than one) so the output does not just echo whichever
    // half of `state` a single SplitMix64 step happens to preserve most strongly.
    SplitMix64(state);
    return SplitMix64(state);
}

}  // namespace

std::uint64_t SplitMix64(std::uint64_t& state) {
    state += kSplitMix64Gamma;
    std::uint64_t z = state;
    z = (z ^ (z >> 30)) * kSplitMix64Mix1;
    z = (z ^ (z >> 27)) * kSplitMix64Mix2;
    return z ^ (z >> 31);
}

std::uint64_t DeriveGalaxySeed(std::uint64_t universeSeed, Coordinate galaxyCoordinate) {
    return DeriveChildSeed(universeSeed, galaxyCoordinate);
}

std::uint64_t DeriveSolarSystemSeed(std::uint64_t galaxySeed, Coordinate systemCoordinate) {
    return DeriveChildSeed(galaxySeed, systemCoordinate);
}

std::uint64_t DeriveCelestialSeed(std::uint64_t solarSystemSeed, Coordinate orbitCoordinate) {
    return DeriveChildSeed(solarSystemSeed, orbitCoordinate);
}

}  // namespace sr::core::galaxy
