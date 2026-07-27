#pragma once

#include <entt/entity/entity.hpp>

namespace sr {

// A body whose WorldTransform is fully determined by orbital motion, not by PhysicsSystem.
//
// `phase` and `angularSpeed` make position a pure function of elapsed simulation time
// (`tick * dt`), never an accumulated `angle += speed * dt`. That is what lets a system fast
// forward across an arbitrary tick gap -- warping back into a system after it sat unticked --
// by evaluating the equation once at the target tick instead of replaying every intermediate one
// (architecture.md Law 2).
struct OrbitBody {
    // entt::null means the system's fixed origin (the sun, in every system generated so far).
    // Otherwise another entity with either an OrbitBody (a moon orbiting a planet) or a fixed
    // WorldTransform. Live-registry-local like Rig::children/ParentRig::root -- never persisted.
    entt::entity center = entt::null;

    float radius = 0.0f;        // World units from center.
    float angularSpeed = 0.0f;  // Radians per second; sign gives direction.
    float phase = 0.0f;         // Radians, the angle at tick 0.
};

// Attached to a mass that pulls nearby free-flying bodies inward -- in practice, a system's sun.
// Bodies with OrbitBody are exempt (their WorldTransform is already fully determined above);
// this only nudges the Velocity of things PhysicsSystem still integrates, such as ships.
struct GravityWell {
    float range = 0.0f;     // Beyond this distance the well has no effect.
    float strength = 0.0f;  // Acceleration at the well's center, falling off toward `range`.
};

}  // namespace sr
