#pragma once

#include <cstdint>

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

// Set by CollisionSystem after a rig takes ramming damage; gates repeat damage every tick while
// two hulls are still overlapping, until the ships have separated and reapproached.
struct RamCooldown {
    float secondsRemaining = 0.0f;
};

// Declaration order is draw order AMONG WORLD BODIES, back to front. It is NOT the world's full
// draw order -- rigs and projectiles are drawn by their own passes and always sit in front of
// every one of these. WorldRenderer::DrawWorld states the full order in one place; see it before
// adding an enumerator. Same convention as BridgeView::kAllKinds, and the same hazard, so the
// renderer's switch over this must stay exhaustive with no default.
//
// Anomaly has no spawn site yet (lands with P5-06's Act I prologue) but is added here rather than
// later: features.md 1.2's prologue turns one on, lore.md 2 makes anomalies a recurring source of
// rare tech, and adding an enumerator later means revisiting every exhaustive switch instead of
// one now.
enum class BodyKind : std::uint8_t { Star, Planet, Wreck, Drop, Asteroid, Anomaly };

// A world object that is NOT a rig -- a star, a planet, an asteroid, a dropped item, a wreck: one
// entity, one shape, no hardpoints, no hierarchy. Vessels and stations are deliberately absent,
// and not because they are a missing enumerator: a rig is not one drawable (a root plus N
// hardpoint entities), and giving a root a radius here would put it alongside CollisionRadius as
// a second record of the same fact.
struct WorldBody {
    float radius = 1.0f;
    BodyKind kind = BodyKind::Asteroid;
};

// The lethal volume around a star. Mirrors GravityWell (Orbit.h) field for field and uses the
// SAME falloff, deliberately: one shape, two effects, both centred on the same entity.
struct Corona {
    float range = 0.0f;            // Outer edge, world units from centre. Damage is zero here.
    float damagePerSecond = 0.0f;  // At the centre, scaled by (1 - d/range)^2 outward.
};

}  // namespace sr
