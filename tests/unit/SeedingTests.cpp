#include <catch2/catch_test_macros.hpp>

#include "core/galaxy/Seeding.h"

using sr::core::galaxy::Coordinate;
using sr::core::galaxy::DeriveCelestialSeed;
using sr::core::galaxy::DeriveGalaxySeed;
using sr::core::galaxy::DeriveSolarSystemSeed;
using sr::core::galaxy::SplitMix64;

// Every expected value below is hardcoded rather than computed by the test itself
// (architecture.md 12.4's requirement) -- an independent implementation of this file's algorithm
// on another platform must land on the same numbers, or CI catches it.

TEST_CASE("SplitMix64 matches its pinned reference sequence from state zero", "[seeding]") {
    std::uint64_t state = 0;
    CHECK(SplitMix64(state) == 0xE220A8397B1DCDAFULL);
    CHECK(SplitMix64(state) == 0x6E789E6AA1B965F4ULL);
    CHECK(SplitMix64(state) == 0x06C45D188009454FULL);
}

TEST_CASE("DeriveGalaxySeed matches its pinned expected value", "[seeding]") {
    constexpr std::uint64_t kUniverseSeed = 0x1234567890ABCDEFULL;
    CHECK(DeriveGalaxySeed(kUniverseSeed, Coordinate{5, 5}) == 0xC2D44A35421B4B65ULL);
    CHECK(DeriveGalaxySeed(kUniverseSeed, Coordinate{-7, -9}) == 0x846AE9E1810001F5ULL);
}

TEST_CASE("DeriveSolarSystemSeed matches its pinned expected value", "[seeding]") {
    constexpr std::uint64_t kGalaxySeed = 0xC2D44A35421B4B65ULL;
    CHECK(DeriveSolarSystemSeed(kGalaxySeed, Coordinate{3, -3}) == 0x1B67BF78A7726E80ULL);
}

TEST_CASE("DeriveCelestialSeed matches its pinned expected value", "[seeding]") {
    constexpr std::uint64_t kSolarSystemSeed = 0x1B67BF78A7726E80ULL;
    CHECK(DeriveCelestialSeed(kSolarSystemSeed, Coordinate{2, 0}) == 0x496FF3797247EB1AULL);
}

TEST_CASE("Same seed and coordinate always produce the same child seed", "[seeding]") {
    constexpr std::uint64_t kUniverseSeed = 0x1234567890ABCDEFULL;
    const std::uint64_t first = DeriveGalaxySeed(kUniverseSeed, Coordinate{42, -17});
    const std::uint64_t second = DeriveGalaxySeed(kUniverseSeed, Coordinate{42, -17});
    CHECK(first == second);
}

TEST_CASE("Generation is order-independent -- deriving other coordinates first changes nothing",
          "[seeding]") {
    constexpr std::uint64_t kUniverseSeed = 0x1234567890ABCDEFULL;
    const std::uint64_t bBeforeC = DeriveGalaxySeed(kUniverseSeed, Coordinate{1, 1});
    (void)DeriveGalaxySeed(kUniverseSeed, Coordinate{2, 2});
    (void)DeriveGalaxySeed(kUniverseSeed, Coordinate{3, 3});

    // Visiting the same coordinate again, after two others were derived in between, must yield
    // the identical result -- nothing accumulates across calls (features.md 7.1).
    const std::uint64_t bAfterC = DeriveGalaxySeed(kUniverseSeed, Coordinate{1, 1});
    CHECK(bBeforeC == bAfterC);
}

TEST_CASE("Different coordinates under the same parent produce different seeds", "[seeding]") {
    constexpr std::uint64_t kUniverseSeed = 0x1234567890ABCDEFULL;
    const std::uint64_t at00 = DeriveGalaxySeed(kUniverseSeed, Coordinate{0, 0});
    const std::uint64_t at01 = DeriveGalaxySeed(kUniverseSeed, Coordinate{0, 1});
    CHECK(at00 != at01);
}

TEST_CASE("Different parent seeds produce different children for the same coordinate",
          "[seeding]") {
    const std::uint64_t fromUniverse = DeriveGalaxySeed(1, Coordinate{5, 5});
    const std::uint64_t fromDifferentUniverse = DeriveGalaxySeed(2, Coordinate{5, 5});
    CHECK(fromUniverse != fromDifferentUniverse);
}
