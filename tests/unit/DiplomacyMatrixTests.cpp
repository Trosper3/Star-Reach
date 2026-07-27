#include <catch2/catch_test_macros.hpp>

#include "core/diplomacy/DiplomacyMatrix.h"

using sr::FactionId;
using sr::core::diplomacy::DiplomacyMatrix;
using sr::core::diplomacy::Relation;

TEST_CASE("DiplomacyMatrix defaults unset pairs to Neutral", "[diplomacy]") {
    DiplomacyMatrix matrix;
    CHECK(matrix.Get(FactionId("aegis"), FactionId("reavers")) == Relation::Neutral);
}

TEST_CASE("DiplomacyMatrix treats a faction as always Friendly with itself", "[diplomacy]") {
    DiplomacyMatrix matrix;
    matrix.Set(FactionId("aegis"), FactionId("aegis"), Relation::Hostile);
    CHECK(matrix.Get(FactionId("aegis"), FactionId("aegis")) == Relation::Friendly);
}

TEST_CASE("DiplomacyMatrix Set is readable from either direction", "[diplomacy]") {
    DiplomacyMatrix matrix;
    matrix.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    CHECK(matrix.Get(FactionId("aegis"), FactionId("reavers")) == Relation::Hostile);
    CHECK(matrix.Get(FactionId("reavers"), FactionId("aegis")) == Relation::Hostile);
}

TEST_CASE("DiplomacyMatrix Set overwrites a prior relation", "[diplomacy]") {
    DiplomacyMatrix matrix;
    matrix.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    matrix.Set(FactionId("reavers"), FactionId("aegis"), Relation::Friendly);
    CHECK(matrix.Get(FactionId("aegis"), FactionId("reavers")) == Relation::Friendly);
}

TEST_CASE("DiplomacyMatrix keeps unrelated pairs independent", "[diplomacy]") {
    DiplomacyMatrix matrix;
    matrix.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    CHECK(matrix.Get(FactionId("aegis"), FactionId("republic")) == Relation::Neutral);
    CHECK(matrix.Get(FactionId("reavers"), FactionId("republic")) == Relation::Neutral);
}
