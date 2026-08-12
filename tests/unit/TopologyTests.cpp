#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>

#include "core/galaxy/Seeding.h"
#include "core/galaxy/Topology.h"

using sr::core::galaxy::CoordFromId;
using sr::core::galaxy::DeriveSolarSystemSeed;
using sr::core::galaxy::IdFromCoord;
using sr::core::galaxy::SystemCoord;
using sr::core::galaxy::SystemId;
using sr::core::galaxy::SystemRecord;
using sr::core::galaxy::Topology;

// Every expected value below is hardcoded rather than computed by the test itself
// (architecture.md 12.4's discipline, required here by 12.17 too) -- an independent
// implementation of IdFromCoord's bit-pack on another platform must land on the same numbers, or
// CI catches it.

TEST_CASE("IdFromCoord matches its pinned expected value", "[topology]") {
    CHECK(IdFromCoord(SystemCoord{0, 0}) == 0x0000000000000000ULL);
    CHECK(IdFromCoord(SystemCoord{1, 0}) == 0x0000000100000000ULL);
    CHECK(IdFromCoord(SystemCoord{0, 1}) == 0x0000000000000001ULL);
    CHECK(IdFromCoord(SystemCoord{-1, -1}) == 0xFFFFFFFFFFFFFFFFULL);
    CHECK(IdFromCoord(SystemCoord{5, -3}) == 0x00000005FFFFFFFDULL);
}

TEST_CASE("Coordinate and id round-trip through IdFromCoord/CoordFromId", "[topology]") {
    const SystemCoord original{42, -17};
    const SystemId id = IdFromCoord(original);
    const SystemCoord roundTripped = CoordFromId(id);

    CHECK(roundTripped.x == original.x);
    CHECK(roundTripped.y == original.y);
}

TEST_CASE("Round-trip holds at the int32 extremes", "[topology]") {
    const SystemCoord original{2147483647, -2147483647 - 1};  // INT32_MAX, INT32_MIN.
    CHECK(CoordFromId(IdFromCoord(original)).x == original.x);
    CHECK(CoordFromId(IdFromCoord(original)).y == original.y);
}

TEST_CASE("The same coordinate always yields the same id", "[topology]") {
    const SystemCoord coord{7, 9};
    CHECK(IdFromCoord(coord) == IdFromCoord(coord));
}

TEST_CASE("Distinct coordinates yield distinct ids", "[topology]") {
    CHECK(IdFromCoord(SystemCoord{1, 2}) != IdFromCoord(SystemCoord{2, 1}));
}

TEST_CASE("A system with no record still resolves to an id a seed can be derived from",
          "[topology]") {
    // architecture.md 12.17: identity is location-derived, so a never-visited system needs no
    // stored record to produce a seed -- IdFromCoord and the coordinate itself are usable
    // unconditionally, and Topology confirms it holds no record for it.
    Topology topology;
    const SystemCoord coord{123, -456};

    CHECK(topology.Find(IdFromCoord(coord)) == nullptr);

    constexpr std::uint64_t kGalaxySeed = 0x1234567890ABCDEFULL;
    const std::uint64_t seed = DeriveSolarSystemSeed(kGalaxySeed, coord);
    (void)seed;  // No crash, no record required -- that is the whole assertion.
}

TEST_CASE("Neighbors returns the 8 grid cells surrounding a coordinate", "[topology]") {
    const Topology topology;
    const std::vector<SystemCoord> neighbors = topology.Neighbors(SystemCoord{0, 0});

    REQUIRE(neighbors.size() == 8);
    for (const SystemCoord& n : neighbors) {
        CHECK_FALSE((n.x == 0 && n.y == 0));  // Never includes the origin itself.
        CHECK(std::abs(n.x) <= 1);
        CHECK(std::abs(n.y) <= 1);
    }
}

TEST_CASE("Neighbor queries are symmetric", "[topology]") {
    const Topology topology;
    const SystemCoord a{10, 10};

    for (const SystemCoord& b : topology.Neighbors(a)) {
        const std::vector<SystemCoord> neighborsOfB = topology.Neighbors(b);
        const bool aIsNeighborOfB =
            std::find_if(neighborsOfB.begin(), neighborsOfB.end(), [&](const SystemCoord& c) {
                return c.x == a.x && c.y == a.y;
            }) != neighborsOfB.end();
        CHECK(aIsNeighborOfB);
    }
}

TEST_CASE("GetOrCreate stores a record findable by the coordinate's id", "[topology]") {
    Topology topology;
    const SystemCoord coord{3, 4};

    SystemRecord& record = topology.GetOrCreate(coord);
    record.displayName = "Sol";

    const SystemRecord* found = topology.Find(IdFromCoord(coord));
    REQUIRE(found != nullptr);
    CHECK(found->displayName == "Sol");
    CHECK(found->coord.x == coord.x);
    CHECK(found->coord.y == coord.y);
}

TEST_CASE("GetOrCreate does not overwrite an existing record", "[topology]") {
    Topology topology;
    const SystemCoord coord{1, 1};

    topology.GetOrCreate(coord).displayName = "Named";
    topology.GetOrCreate(coord);  // Touched again.

    CHECK(topology.Find(IdFromCoord(coord))->displayName == "Named");
}

TEST_CASE("A freshly claimed record is unclaimed by default", "[topology]") {
    Topology topology;
    CHECK(topology.GetOrCreate(SystemCoord{0, 0}).claimedBy.empty());
}

TEST_CASE("Erase removes a record, and Find returns nullptr afterward", "[topology]") {
    Topology topology;
    const SystemCoord coord{8, 8};
    const SystemId id = IdFromCoord(coord);

    topology.GetOrCreate(coord);
    REQUIRE(topology.Find(id) != nullptr);

    topology.Erase(id);
    CHECK(topology.Find(id) == nullptr);
}

TEST_CASE("Erase on an id with no record is a no-op", "[topology]") {
    Topology topology;
    topology.Erase(IdFromCoord(SystemCoord{99, 99}));  // Must not throw or crash.
    CHECK(topology.Records().empty());
}

TEST_CASE("Records() reflects exactly what has been touched", "[topology]") {
    Topology topology;
    CHECK(topology.Records().empty());

    topology.GetOrCreate(SystemCoord{1, 0});
    topology.GetOrCreate(SystemCoord{0, 1});
    CHECK(topology.Records().size() == 2);
}
