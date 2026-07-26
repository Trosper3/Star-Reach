#pragma once

#include "shared/math/Vec2.h"

namespace sr {

// On rig roots only. Hardpoints do not move independently -- they inherit through
// HierarchySystem.
struct Velocity {
    Vec2 linear;
    float angular = 0.0f;
};

// Total rig mass, summed from shells and modules at instantiation and recomputed on refit.
//
// This is real F = ma, not a fudge factor: mass is the output of the loadout puzzle
// (features.md section 2.2), so it has to be the actual denominator in acceleration or the
// puzzle has no teeth.
struct BodyMass {
    float kilograms = 1.0f;
};

// Aggregate propulsion available to the rig, summed over its living engine hardpoints.
// PowerSystem gates it and DamageSystem invalidates it -- when the last engine shell dies these
// go to zero and the ship stalls.
struct Propulsion {
    float thrustNewtons = 0.0f;
    float turnTorque = 0.0f;
    float maxSpeed = 0.0f;
};

// This tick's control input, in the range [-1, 1]. Written by NpcAiSystem or by player input
// (as an intent, Law 9), consumed by PhysicsSystem. Nothing else writes it, and it is cleared
// every tick so a dead controller coasts rather than holding the throttle open.
struct ThrustInput {
    float forward = 0.0f;
    float strafe = 0.0f;
    float turn = 0.0f;
};

// Space drag is not physical; it is a handling knob. Keeping it an explicit component rather
// than a constant inside PhysicsSystem means a hull can tune it, and means the value is visible
// in a save rather than hidden in code.
struct LinearDamping {
    float perSecond = 0.0f;
    float angularPerSecond = 0.0f;
};

// Broad-phase radius for the spatial grid. Narrow-phase convex hulls (ported from StarReach2's
// CollisionHull.cpp) refine it; this is only the cheap reject.
struct CollisionRadius {
    float value = 0.0f;
};

}  // namespace sr
