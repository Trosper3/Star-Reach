#include <catch2/catch_test_macros.hpp>

#include "core/diplomacy/Reputation.h"

using sr::FactionId;
using sr::core::diplomacy::Relation;
using sr::core::diplomacy::Reputation;

TEST_CASE("Reputation starts every faction at zero", "[diplomacy]") {
    Reputation reputation;
    CHECK(reputation.Score(FactionId("aegis")) == 0.0f);
    CHECK(reputation.ThresholdRelation(FactionId("aegis")) == Relation::Neutral);
}

TEST_CASE("Reputation accumulates adjustments", "[diplomacy]") {
    Reputation reputation;
    reputation.Adjust(FactionId("aegis"), 10.0f);
    reputation.Adjust(FactionId("aegis"), 20.0f);
    CHECK(reputation.Score(FactionId("aegis")) == 30.0f);
}

TEST_CASE("Reputation keeps each faction independent", "[diplomacy]") {
    Reputation reputation;
    reputation.Adjust(FactionId("aegis"), 30.0f);
    reputation.Adjust(FactionId("reavers"), -30.0f);
    CHECK(reputation.Score(FactionId("aegis")) == 30.0f);
    CHECK(reputation.Score(FactionId("reavers")) == -30.0f);
}

TEST_CASE("Reputation crosses the friendly threshold", "[diplomacy]") {
    Reputation reputation;
    reputation.Adjust(FactionId("aegis"), Reputation::kFriendlyThreshold);
    CHECK(reputation.ThresholdRelation(FactionId("aegis")) == Relation::Friendly);
}

TEST_CASE("Reputation crosses the hostile threshold", "[diplomacy]") {
    Reputation reputation;
    reputation.Adjust(FactionId("aegis"), Reputation::kHostileThreshold);
    CHECK(reputation.ThresholdRelation(FactionId("aegis")) == Relation::Hostile);
}

TEST_CASE("Reputation Tick decays a positive score toward zero without overshooting",
          "[diplomacy]") {
    Reputation reputation;
    reputation.Adjust(FactionId("aegis"), 5.0f);
    reputation.Tick(100.0f);
    CHECK(reputation.Score(FactionId("aegis")) == 0.0f);
}

TEST_CASE("Reputation Tick decays a negative score toward zero without overshooting",
          "[diplomacy]") {
    Reputation reputation;
    reputation.Adjust(FactionId("aegis"), -5.0f);
    reputation.Tick(100.0f);
    CHECK(reputation.Score(FactionId("aegis")) == 0.0f);
}

TEST_CASE("Reputation Tick ignores a non-positive dt", "[diplomacy]") {
    Reputation reputation;
    reputation.Adjust(FactionId("aegis"), 5.0f);
    reputation.Tick(0.0f);
    reputation.Tick(-1.0f);
    CHECK(reputation.Score(FactionId("aegis")) == 5.0f);
}
