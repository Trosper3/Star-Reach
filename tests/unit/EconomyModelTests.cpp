#include <catch2/catch_test_macros.hpp>

#include "economy_sim/EconomyModel.h"

// tools/economy_sim -- see EconomyModel.h. These pin down the model against the exact figures
// architecture.md 12.19 and features.md 2.8 already cite by hand, so a change to the model is
// caught the moment it stops reproducing numbers this project has already agreed on.

using sr::economy_sim::CostModel;
using sr::economy_sim::Grade;

TEST_CASE("SlotBreadth spans 2 to 8 linearly across the ladder", "[economy_sim]") {
    REQUIRE(sr::economy_sim::SlotBreadth(Grade::Common) == 2);
    REQUIRE(sr::economy_sim::SlotBreadth(Grade::Mythic) == 8);
    REQUIRE(sr::economy_sim::SlotBreadth(Grade::Legendary) == 7);
}

TEST_CASE("QualityBandUpper matches features.md 2.7's table", "[economy_sim]") {
    REQUIRE(sr::economy_sim::QualityBandUpper(Grade::Common) == 1.10);
    REQUIRE(sr::economy_sim::QualityBandUpper(Grade::Mythic) == 5.00);
}

TEST_CASE("MaterialBaseValue reproduces architecture.md 12.19's 'themselves x256' figure",
          "[economy_sim]") {
    // No margin, so the ratio isolates the two knobs a Material carries: quantity-per-grade and
    // slot breadth. At the doc's own working value (quantityPerGrade = 2.0) this must be exactly
    // 256, which is architecture.md 12.19's cited figure for a Material's own grade span.
    CostModel model;
    model.quantityPerGrade = 2.0;
    model.marginRate = 0.0;

    const double common = sr::economy_sim::MaterialBaseValue(Grade::Common, model);
    const double mythic = sr::economy_sim::MaterialBaseValue(Grade::Mythic, model);

    REQUIRE(common == 2.0);
    REQUIRE(mythic == 512.0);
    REQUIRE(mythic / common == 256.0);
}

TEST_CASE("ModuleBaseValue's Common->Mythic multiplier is order 10^4 at the doc's working value",
          "[economy_sim]") {
    // architecture.md 12.19: "a Mythic module costs on the order of 10^4 Common modules rather
    // than 64" once all three knobs (quantity, breadth, input-grade chain) compound together.
    CostModel model;
    model.quantityPerGrade = 2.0;
    model.marginRate = 0.0;

    const double common = sr::economy_sim::ModuleBaseValue(Grade::Common, model);
    const double mythic = sr::economy_sim::ModuleBaseValue(Grade::Mythic, model);
    const double ratio = mythic / common;

    REQUIRE(ratio > 1000.0);
    REQUIRE(ratio < 100000.0);
}

TEST_CASE("Dropping quantityPerGrade toward 1.0 tames the curve, per features.md 2.10's own note",
          "[economy_sim]") {
    // features.md 2.10: "The ~2x figure was chosen against a one-knob model. Against three it is
    // probably nearer ~1x." This is the check that claim is actually true of the model.
    CostModel twoX;
    twoX.quantityPerGrade = 2.0;
    twoX.marginRate = 0.0;

    CostModel oneX;
    oneX.quantityPerGrade = 1.0;
    oneX.marginRate = 0.0;

    const double ratioAtTwoX = sr::economy_sim::ModuleBaseValue(Grade::Mythic, twoX) /
                               sr::economy_sim::ModuleBaseValue(Grade::Common, twoX);
    const double ratioAtOneX = sr::economy_sim::ModuleBaseValue(Grade::Mythic, oneX) /
                                sr::economy_sim::ModuleBaseValue(Grade::Common, oneX);

    REQUIRE(ratioAtOneX < ratioAtTwoX);
    // features.md 2.4's hard constraint: cost must still outpace the ~x6 combat-value ceiling
    // even at the gentlest candidate.
    REQUIRE(ratioAtOneX > sr::economy_sim::QualityBandUpper(Grade::Mythic));
}

TEST_CASE("ManufacturingTimeSeconds matches features.md 2.8's already-settled table exactly",
          "[economy_sim]") {
    REQUIRE(sr::economy_sim::ManufacturingTimeSeconds(Grade::Common, false) == 5.0);
    REQUIRE(sr::economy_sim::ManufacturingTimeSeconds(Grade::Mythic, false) == 320.0);   // 5m20s
    REQUIRE(sr::economy_sim::ManufacturingTimeSeconds(Grade::Common, true) == 10.0);
    REQUIRE(sr::economy_sim::ManufacturingTimeSeconds(Grade::Mythic, true) == 640.0);    // 10m40s
}
