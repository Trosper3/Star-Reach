#pragma once

#include <cstddef>

// tools/economy_sim -- headless derivation of the manufacturing cost curve across the seven-tier
// grade ladder (features.md sections 2.7/2.10). features.md 2.10 identifies three independently
// compounding cost knobs (quantity per grade, slot-count breadth 2->8, and the input-grade supply
// chain) and states outright that "a curve with three compounding terms cannot be read by
// inspection" -- this runs the derivation instead of hand-estimating it.
//
// No dependency on sr_shared/sr_core content types, on purpose: no elements.json or
// materials.json exists yet (architecture.md 14.2), so this models the SHAPE of the curve from
// the grade table's own authored rules (slot count, the flat 1-credit element base value) rather
// than reading real content. Once a real ItemInstance/recipe type and content files exist, this
// is the natural place to grow a real-content code path alongside the parametric one -- see
// architecture.md 13.5 group 2c for that follow-on (core/economy/Pricing.h).
namespace sr::economy_sim {

// features.md 2.7's seven-tier ladder, index 0..6.
enum class Grade : int {
    Common = 0,
    Uncommon = 1,
    Unique = 2,
    Rare = 3,
    Epic = 4,
    Legendary = 5,
    Mythic = 6,
};

inline constexpr int kGradeCount = 7;

const char* GradeName(Grade grade);

// features.md 2.10: "A Material's recipe is n slots, where n is the grade's distinct count
// (2->8)." Linear across the seven tiers -- Common=2 ... Mythic=8 -- the only table that fits
// "2->8" literally over seven steps.
int SlotBreadth(Grade grade);

// features.md 2.7's quality-band table, upper bound per tier -- used here as a combat-value
// proxy, per 2.10's own framing ("Mythic's actual delivery of roughly x6 combat value").
double QualityBandUpper(Grade grade);

// Parameters the three-knob model is tuned by. quantityPerGrade defaults to the doc's own
// working value (~2.0, features.md 2.10); marginRate is the "modest, roughly linear" per-step
// markup applied once per production step.
struct CostModel {
    double quantityPerGrade = 2.0;
    double marginRate = 0.15;
};

// features.md 2.10: "One unit of any element is 1 credit. All price differentiation is local."
inline constexpr double kElementBaseValue = 1.0;

// Element -> Material: slotBreadth(grade) x quantityPerGrade^grade x elementBaseValue, plus one
// margin step. At the model's default (quantityPerGrade=2.0, marginRate=0) this reproduces
// architecture.md 12.19's own "themselves x256" figure for Material grade Mythic vs Common
// exactly -- see EconomyModelTests.cpp.
double MaterialBaseValue(Grade grade, const CostModel& model);

// Material -> Module: the input-grade chain (features.md 2.8 -- "a grade-N item needs grade >=
// N-1 Materials"). Recurses into MaterialBaseValue(grade-1) (floored at Common), plus its own
// slot/quantity term and a second margin step.
double ModuleBaseValue(Grade grade, const CostModel& model);

// features.md 2.8's time curve -- already settled, not derived: base 5s (Material) / 10s
// (Module/Shell), doubling per grade. Included so the tool reports one complete picture rather
// than the cost curve alone.
double ManufacturingTimeSeconds(Grade grade, bool isModule);

}  // namespace sr::economy_sim
