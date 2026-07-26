#pragma once

namespace sr {

// On a hardpoint carrying a power cell module.
struct PowerSource {
    float generation = 0.0f;
};

// On any hardpoint that consumes power.
struct PowerLoad {
    float draw = 0.0f;
    // Higher sheds last. PowerSystem browns out low-priority loads first so a damaged rig
    // degrades in a legible order -- facilities before shields before engines -- instead of
    // failing everywhere at once.
    int priority = 0;
};

// On the rig root, recomputed by PowerSystem each tick from living hardpoints only.
//
// StarReach2 computed a power budget and then never read it; the throttle it produced gated
// nothing. The single consumer rule (architecture.md section 2.4) exists because of exactly
// that: if nothing reads `satisfaction`, this component gets deleted rather than kept.
struct PowerBudget {
    float generation = 0.0f;
    float draw = 0.0f;
    // Fraction of demand actually met, in [0, 1]. WeaponSystem scales fire rate by it and
    // PhysicsSystem scales thrust by it; that is what makes losing a power cell felt.
    float satisfaction = 1.0f;
};

// Tag written by PowerSystem onto hardpoints it browned out this tick. Systems check for its
// absence rather than recomputing the budget themselves.
struct PowerShed {};

}  // namespace sr
