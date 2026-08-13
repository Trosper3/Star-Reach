#pragma once

#include <entt/entity/entity.hpp>

#include "shared/math/Vec2.h"

namespace sr {

// The rig this entity is currently engaging, plus the specific hardpoint it is aiming at.
//
// Two fields rather than one because the design is surgical: the player and the AI both pick a
// *rig* to fight and a *hardpoint* to remove (features.md section 3.2). Collapsing them loses
// the ability to keep tracking a target whose selected subsystem just died.
struct Target {
    entt::entity rig = entt::null;
    entt::entity hardpoint = entt::null;
};

// Tag: this rig may be acquired by TargetingSystem. Absent on wrecks, loot, and docked craft --
// a docked player is untargetable, which is the rule the legacy game had to retrofit.
struct Targetable {};

// Sensor reach of a rig, used for acquisition range and for the intel model in
// DiscoverySystem. Not the same as weapon range: you can see further than you can shoot.
struct SensorRange {
    float units = 0.0f;
};

// The cursor's current world position. features.md section 3.2: the player aims manually, there
// is no target lock -- turrets slew toward this rather than toward an acquired Target. Written
// every frame on the player's rig root by PlayerInputSystem, draining the AimIntent FlightControls
// pushes (Law 9: the input producer never writes a component itself). NPC rigs never carry this
// and keep resolving through Target -- WeaponSystem::AimPointPosition prefers AimPoint when
// present and falls back to Target otherwise, one function with one branch.
struct AimPoint {
    Vec2 world;
};

}  // namespace sr
