#pragma once

#include <cstdint>
#include <optional>

#include <entt/entity/entity.hpp>

#include "shared/blueprints/Ids.h"
#include "shared/blueprints/ModuleDef.h"
#include "shared/blueprints/Taxonomy.h"
#include "shared/math/Vec2.h"

namespace sr {

// On a hardpoint carrying a weapon module. Destroying this hardpoint removes the firing arc
// permanently (features.md section 3.2) -- there is no rig-level weapon list to fall back on.
struct Weapon {
    // --- Authored, copied from WeaponStats at attach (shared/blueprints/ModuleDef.h carries the
    // full comment on what each behavior modifier means and how they compose). ---
    float damage = 0.0f;
    DamageType damageType = DamageType::Kinetic;
    float fireIntervalSeconds = 1.0f;
    float projectileSpeed = 0.0f;
    float rangeUnits = 0.0f;
    float spreadRadians = 0.0f;
    int projectilesPerShot = 1;
    bool continuous = false;
    float homingTurnRatePerSecond = 0.0f;
    bool chargeToFire = false;
    float chargeSecondsToFire = 0.0f;
    int burstCount = 1;
    float burstIntervalSeconds = 0.0f;
    float maxAmmo = -1.0f;
    float consistency = 1.0f;
    float accuracyRadians = 0.0f;

    // --- Runtime, mutated by WeaponSystem. ---

    // Counts down; the mount may fire at zero. Stored per hardpoint, so a rig with four turrets
    // has four independent cooldowns and loses exactly one when a turret dies.
    float cooldown = 0.0f;

    // Initialized from maxAmmo at attach; -1 (matching an unlimited maxAmmo) never gates firing.
    float ammoRemaining = -1.0f;

    // Seconds accumulated toward chargeSecondsToFire this hold; reset to zero the instant
    // FireIntent is absent (WeaponSystem::Tick), so a charge never survives a release.
    float chargeSeconds = 0.0f;

    // Set once, on the tick a chargeToFire+homing hold begins, to whatever was under the cursor
    // at that instant (WeaponSystem's lock-acquisition step); entt::null for every other weapon,
    // including a plain (non-charge) homing weapon, which re-seeks the nearest target every tick
    // instead of remembering one.
    entt::entity lockedTarget = entt::null;

    // Mid-burst countdown: shots left to fire in the burst currently playing out, and seconds
    // until the next one. Zero/zero means "not mid-burst."
    int burstShotsRemaining = 0;
    float burstTimer = 0.0f;

    // Authored (copied from WeaponStats::colorOverride at attach), but appended here rather than
    // grouped with the other authored fields above: shared/rig/ModuleAttachment.cpp's
    // AttachWeapon is the only production constructor, but several tests build a Weapon
    // positionally too (tests/unit/WeaponSystemTests.cpp) -- a field inserted in the MIDDLE of
    // this struct has already silently mis-assigned a positional test's trailing arguments once
    // this same session (consistency/accuracyRadians landing in what a test still thought were
    // cooldown/ammoRemaining). Appending instead of inserting means any positional list shorter
    // than the full field count -- every existing one -- is unaffected by this field's addition.
    std::optional<ColorRGBA> colorOverride;
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

    // Distance actually covered since the muzzle, world units. WorldRenderer's tracer clamps its
    // fixed-length tail to this -- a shot only 1-2 ticks old cannot yet have a tail as long as
    // kProjectileTracerSeconds' worth of travel, and drawing one anyway overshoots back past the
    // muzzle, reading as originating from behind the rig that fired it.
    float distanceTraveled = 0.0f;

    // Copied from Weapon::homingTurnRatePerSecond at spawn; 0 means this projectile just flies
    // the straight line WeaponSystem gave it. ProjectileSystem::Tick steers `Velocity::linear`
    // toward `homingTarget` (or, absent one, the nearest eligible hardpoint) at this rate before
    // integrating position each tick.
    float homingTurnRatePerSecond = 0.0f;

    // entt::null at spawn for a plain (non-charge) homing weapon -- ProjectileSystem re-seeks the
    // nearest eligible hardpoint every tick, never remembering one, which is what makes it a
    // dumb-fire seeker rather than a lock. Non-null for a weapon that acquired an explicit lock
    // (Weapon::lockedTarget, chargeToFire+homing): this projectile chases that one specific
    // entity and, per `homingWasLocked` below, goes permanently dumb (stops steering) rather than
    // opportunistically retargeting if it dies mid-flight.
    entt::entity homingTarget = entt::null;

    // True if `homingTarget` came from an acquired lock rather than "seek nearest, always." A
    // locked missile whose target dies flies straight from then on; a dumb-fire seeker keeps
    // looking for something else instead.
    bool homingWasLocked = false;

    // Copied from Weapon::colorOverride at spawn (a Projectile outlives its Weapon's hardpoint
    // dying mid-flight, so it cannot read the override back off it later) -- WorldRenderer draws
    // this instead of DamageTypeColor(damageType) when present.
    std::optional<ColorRGBA> colorOverride;
};

// Written by WeaponSystem on a hardpoint every tick a `Weapon::continuous` mount actually fires
// (present == firing, not merely powered/aimed); removed the instant it does not, so a stale
// endpoint from a tick ago is never mistaken for a live beam. WorldRenderer reads this to draw
// the beam line -- a continuous weapon has no Projectile entity for a render pass to key off, per
// WeaponStats::continuous's own comment.
struct BeamState {
    Vec2 endPoint;
    DamageType damageType = DamageType::Kinetic;
    // Copied from Weapon::colorOverride each tick FireContinuous writes this. Same reason as
    // Projectile::colorOverride: WorldRenderer draws this instead of DamageTypeColor(damageType)
    // when present.
    std::optional<ColorRGBA> colorOverride;
};

}  // namespace sr
