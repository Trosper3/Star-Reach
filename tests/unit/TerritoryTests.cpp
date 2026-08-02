#include <catch2/catch_test_macros.hpp>

#include "core/diplomacy/Territory.h"

using sr::FactionId;
using sr::core::diplomacy::Territory;

TEST_CASE("Territory reports an unclaimed system as owned by the empty faction", "[diplomacy]") {
    Territory territory;
    CHECK(territory.Owner("sol").empty());
}

TEST_CASE("Territory Claim assigns an owner", "[diplomacy]") {
    Territory territory;
    territory.Claim("sol", FactionId("aegis"));
    CHECK(territory.Owner("sol") == FactionId("aegis"));
}

TEST_CASE("Territory Claim overwrites a prior owner", "[diplomacy]") {
    Territory territory;
    territory.Claim("sol", FactionId("aegis"));
    territory.Claim("sol", FactionId("reavers"));
    CHECK(territory.Owner("sol") == FactionId("reavers"));
}

TEST_CASE("Territory Release returns a system to unclaimed", "[diplomacy]") {
    Territory territory;
    territory.Claim("sol", FactionId("aegis"));
    territory.Release("sol");
    CHECK(territory.Owner("sol").empty());
}

TEST_CASE("Territory keeps systems independent", "[diplomacy]") {
    Territory territory;
    territory.Claim("sol", FactionId("aegis"));
    territory.Claim("proxima", FactionId("reavers"));
    CHECK(territory.Owner("sol") == FactionId("aegis"));
    CHECK(territory.Owner("proxima") == FactionId("reavers"));
}

TEST_CASE("Territory ReleaseAll unclaims every system a faction owns, leaving others intact",
          "[diplomacy]") {
    Territory territory;
    territory.Claim("sol", FactionId("reavers"));
    territory.Claim("kepler", FactionId("reavers"));
    territory.Claim("vega", FactionId("aegis"));

    territory.ReleaseAll(FactionId("reavers"));

    CHECK(territory.Owner("sol").empty());
    CHECK(territory.Owner("kepler").empty());
    CHECK(territory.Owner("vega") == FactionId("aegis"));
}

TEST_CASE("Territory ReleaseAll on a faction that owns nothing is a no-op", "[diplomacy]") {
    Territory territory;
    territory.Claim("sol", FactionId("aegis"));

    territory.ReleaseAll(FactionId("reavers"));

    CHECK(territory.Owner("sol") == FactionId("aegis"));
}
