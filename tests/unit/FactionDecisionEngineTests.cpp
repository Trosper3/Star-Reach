#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/ai/FactionDecisionEngine.h"

using Catch::Approx;
using sr::FactionId;
using sr::core::ai::ColonizeDirective;
using sr::core::ai::ComputeFacets;
using sr::core::ai::EvaluateColonization;
using sr::core::ai::EvaluateRaidDispatch;
using sr::core::ai::FacetProfile;
using sr::core::ai::kReferenceStock;
using sr::core::ai::RaidDispatchChance;
using sr::core::ai::RaidDispatchDirective;
using sr::core::diplomacy::Territory;
using sr::core::economy::FactionEconomy;

TEST_CASE("ComputeFacets reads Material Security from stock, leaving the rest neutral",
          "[faction-decision]") {
    FactionEconomy economy;
    economy.Deposit(FactionId("aegis"), kReferenceStock);

    const FacetProfile facets = ComputeFacets(economy, FactionId("aegis"));
    CHECK(facets.materialSecurity == Approx(100.0f));
    CHECK(facets.marketDominance == Approx(50.0f));
    CHECK(facets.ideologicalDoctrine == Approx(50.0f));
    CHECK(facets.techSuperiority == Approx(50.0f));
}

TEST_CASE("ComputeFacets clamps Material Security at 0 for a faction with no stock",
          "[faction-decision]") {
    FactionEconomy economy;
    CHECK(ComputeFacets(economy, FactionId("reavers")).materialSecurity == Approx(0.0f));
}

TEST_CASE("RaidDispatchChance is 45% below the Material Security threshold, 0 at or above it",
          "[faction-decision]") {
    FacetProfile low;
    low.materialSecurity = 29.9f;
    CHECK(RaidDispatchChance(low) == Approx(0.45f));

    FacetProfile atThreshold;
    atThreshold.materialSecurity = 30.0f;
    CHECK(RaidDispatchChance(atThreshold) == Approx(0.0f));

    FacetProfile healthy;
    healthy.materialSecurity = 80.0f;
    CHECK(RaidDispatchChance(healthy) == Approx(0.0f));
}

TEST_CASE("EvaluateRaidDispatch fires when the roll lands under the chance", "[faction-decision]") {
    FacetProfile low;
    low.materialSecurity = 10.0f;  // 45% chance.

    const auto fired = EvaluateRaidDispatch(FactionId("reavers"), low, 0.1f);
    REQUIRE(fired.has_value());
    CHECK(fired->faction == FactionId("reavers"));

    CHECK_FALSE(EvaluateRaidDispatch(FactionId("reavers"), low, 0.9f).has_value());
}

TEST_CASE(
    "EvaluateRaidDispatch never fires above the Material Security threshold regardless of roll",
    "[faction-decision]") {
    FacetProfile healthy;
    healthy.materialSecurity = 90.0f;
    CHECK_FALSE(EvaluateRaidDispatch(FactionId("aegis"), healthy, 0.0f).has_value());
}

TEST_CASE("EvaluateColonization proposes an unclaimed system when stock is sufficient",
          "[faction-decision]") {
    FactionEconomy economy;
    economy.Deposit(FactionId("aegis"), kReferenceStock);
    Territory territory;

    const auto directive = EvaluateColonization(FactionId("aegis"), economy, territory, "kepler");
    REQUIRE(directive.has_value());
    CHECK(directive->faction == FactionId("aegis"));
    CHECK(directive->systemId == "kepler");
}

TEST_CASE("EvaluateColonization refuses a system that is already claimed", "[faction-decision]") {
    FactionEconomy economy;
    economy.Deposit(FactionId("aegis"), kReferenceStock);
    Territory territory;
    territory.Claim("kepler", FactionId("reavers"));

    CHECK_FALSE(EvaluateColonization(FactionId("aegis"), economy, territory, "kepler").has_value());
}

TEST_CASE("EvaluateColonization refuses when stock is below the reference amount",
          "[faction-decision]") {
    FactionEconomy economy;
    economy.Deposit(FactionId("aegis"), kReferenceStock - 1);
    Territory territory;

    CHECK_FALSE(EvaluateColonization(FactionId("aegis"), economy, territory, "kepler").has_value());
}
