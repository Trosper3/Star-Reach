#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

#include "shared/blueprints/Ids.h"
#include "shared/blueprints/Taxonomy.h"
#include "shared/math/Vec2.h"

namespace sr {

// On a hardpoint carrying a weapon module. Destroying this hardpoint removes the firing arc
// permanently (features.md section 3.2) -- there is no rig-level weapon list to fall back on.
struct Weapon {
    float damage = 0.0f;
    DamageType damageType = DamageType::Kinetic;
    float fireIntervalSeconds = 1.0f;
    float projectileSpeed = 0.0f;
    float rangeUnits = 0.0f;
    float spreadRadians = 0.0f;
    int projectilesPerShot = 1;

    // Counts down; the mount may fire at zero. Stored per hardpoint, so a rig with four turrets
    // has four independent cooldowns and loses exactly one when a turret dies.
    float cooldown = 0.0f;
};

// Traverse limit for a turret, in radians either side of its mounted facing. Zero means fixed
// forward. TargetingSystem will not select an aim point outside the arc, so a rig can be
// flanked into a blind spot -- which is what makes positioning matter.
struct FiringArc {
    float halfWidthRadians = 0.0f;
    // Current traverse relative to the mount, moved toward the aim point at turnRate.
    float currentOffset = 0.0f;
    float turnRatePerSecond = 0.0f;
};

// Set by input or AI; consumed by WeaponSystem. Cleared every tick.
struct FireIntent {};

// Which of the rig's ten toggleable weapon groups this hardpoint belongs to (features.md 3.6).
// Assigned at spawn: each distinct weapon ModuleId on the rig takes the next free index, so a
// freshly built ship arrives sensibly pre-grouped with no player action.
struct WeaponGroup {
    std::uint8_t index = 0;
};

// Per rig, which of its ten weapon groups currently respond to a fire command -- bit i gates
// WeaponGroup{i}. Session state, not saved (features.md 3.6): a destroyed hardpoint simply stops
// contributing, so no group bookkeeping is needed when one dies. All ten groups start enabled so
// a freshly built ship fires everything until the player deliberately silences one.
struct EnabledWeaponGroups {
    std::uint16_t mask = 0x03FFu;
};

// A live projectile. Its own entity, not a hardpoint -- it has no ParentRig and no rig root.
struct Projectile {
    float damage = 0.0f;
    DamageType damageType = DamageType::Kinetic;
    // Rig root that fired it. Used to skip self-hits and to attribute the relation change in
    // features.md section 5.3.
    entt::entity shooter = entt::null;
    // Distance budget rather than a lifetime, so weapon range is authored in world units and
    // does not silently change when projectile speed is retuned.
    float remainingRange = 0.0f;
};

}  // namespace sr
