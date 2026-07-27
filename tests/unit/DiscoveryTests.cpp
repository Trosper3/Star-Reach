#include <catch2/catch_test_macros.hpp>

#include "core/galaxy/Discovery.h"

using sr::FactionId;
using sr::core::galaxy::DiscoveryState;

TEST_CASE("DiscoveryState starts with nothing discovered", "[discovery]") {
    DiscoveryState state;
    CHECK_FALSE(state.IsDiscovered(FactionId("aegis"), "sol"));
    CHECK(state.Discovered(FactionId("aegis")).empty());
}

TEST_CASE("DiscoveryState records a discovered system", "[discovery]") {
    DiscoveryState state;
    state.Discover(FactionId("aegis"), "sol");
    CHECK(state.IsDiscovered(FactionId("aegis"), "sol"));
    CHECK(state.Discovered(FactionId("aegis")).contains("sol"));
}

TEST_CASE("DiscoveryState is a no-op when a system is discovered twice", "[discovery]") {
    DiscoveryState state;
    state.Discover(FactionId("aegis"), "sol");
    state.Discover(FactionId("aegis"), "sol");
    CHECK(state.Discovered(FactionId("aegis")).size() == 1);
}

TEST_CASE("DiscoveryState keeps each faction's knowledge independent", "[discovery]") {
    DiscoveryState state;
    state.Discover(FactionId("aegis"), "sol");
    state.Discover(FactionId("reavers"), "kepler");

    CHECK(state.IsDiscovered(FactionId("aegis"), "sol"));
    CHECK_FALSE(state.IsDiscovered(FactionId("aegis"), "kepler"));
    CHECK(state.IsDiscovered(FactionId("reavers"), "kepler"));
    CHECK_FALSE(state.IsDiscovered(FactionId("reavers"), "sol"));
}

TEST_CASE("DiscoveryState accumulates multiple systems for one faction", "[discovery]") {
    DiscoveryState state;
    state.Discover(FactionId("aegis"), "sol");
    state.Discover(FactionId("aegis"), "kepler");
    CHECK(state.Discovered(FactionId("aegis")).size() == 2);
}
