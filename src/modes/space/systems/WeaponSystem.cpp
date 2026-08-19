#include "modes/space/systems/WeaponSystem.h"

#include <algorithm>
#include <cmath>
#include <optional>

#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Physics.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"
#include "shared/math/Vec2.h"

namespace sr::space::weapon_system {
namespace {

constexpr float kAimToleranceRadians = 0.03f;

// The world position a rig's hardpoints should aim at. Prefers AimPoint when the rig root carries
// one -- the player's cursor (features.md 3.2: no target lock) -- and falls back to
// TargetingSystem's selected hardpoint (or the rig root it belongs to) otherwise. Nullopt if
// neither is available this tick. One function with one branch, so a player and an NPC rig aim
// through the identical rest of this file.
std::optional<Vec2> AimPointPosition(const entt::registry& registry, entt::entity root,
                                     const Target& target) {
    if (const auto* aim = registry.try_get<AimPoint>(root)) {
        return aim->world;
    }
    const entt::entity point = target.hardpoint != entt::null ? target.hardpoint : target.rig;
    if (point == entt::null || !registry.valid(point)) {
        return std::nullopt;
    }
    const auto* xf = registry.try_get<WorldTransform>(point);
    return xf != nullptr ? std::optional<Vec2>(xf->position) : std::nullopt;
}

// True if `hardpoint`'s weapon group is enabled under `mask`, or if it carries no WeaponGroup at
// all -- a runtime-mounted weapon with no group assigned yet fails open rather than going
// permanently silent (features.md 3.6).
bool GroupEnabled(const entt::registry& registry, entt::entity hardpoint, std::uint16_t mask) {
    const auto* group = registry.try_get<WeaponGroup>(hardpoint);
    return group == nullptr || (mask & (1u << group->index)) != 0;
}

float RigSatisfaction(const entt::registry& registry, entt::entity rigRoot) {
    const auto* budget = registry.try_get<PowerBudget>(rigRoot);
    return budget != nullptr ? budget->satisfaction : 1.0f;
}

// Turns `arc` toward `aimPoint` at its rated traverse speed. Returns true if the mount is
// currently aimed within tolerance of a target that is inside its arc -- i.e. it can hit right
// now, not just eventually.
bool AimAt(FiringArc& arc, const WorldTransform& mountXf, const Vec2& aimPoint, float dt) {
    const float rawOffset = AngleDelta(mountXf.rotation, ToAngle(aimPoint - mountXf.position));
    const bool withinArc = std::abs(rawOffset) <= arc.halfWidthRadians;
    const float desiredOffset = std::clamp(rawOffset, -arc.halfWidthRadians, arc.halfWidthRadians);

    arc.currentOffset = RotateToward(arc.currentOffset, desiredOffset, arc.turnRatePerSecond * dt);

    return withinArc &&
           std::abs(AngleDelta(arc.currentOffset, desiredOffset)) <= kAimToleranceRadians;
}

// Evenly fans `count` pellets across the weapon's spread cone rather than jittering them
// randomly -- a fixed fan keeps fire resolution a pure function of the weapon's stats, which is
// what Law 2's coarse-tick fast-forward needs to replay a tick identically.
float PelletDirection(float baseDirection, float spreadRadians, int index, int count) {
    if (count <= 1) {
        return baseDirection;
    }
    const float t = static_cast<float>(index) / static_cast<float>(count - 1) - 0.5f;
    return baseDirection + spreadRadians * t;
}

// `arc.currentOffset` is the mount's actual physical bearing this tick (relative to mountXf's own
// rotation) -- AimAt integrates it toward the target but gates firing on it, so using it here
// too is what makes "where the shot goes" match "whether the mount may fire." Recomputing a
// fresh, perfect bearing to aimPoint instead (the previous behaviour) discarded currentOffset
// after AimAt had already spent a tick's worth of traverse computing it (architecture.md 13.3
// finding E).
void SpawnProjectiles(entt::registry& registry, entt::entity shooter, const Weapon& weapon,
                      const WorldTransform& mountXf, const FiringArc& arc, float mountRadius) {
    const float baseDirection = mountXf.rotation + arc.currentOffset;
    const int count = std::max(weapon.projectilesPerShot, 1);

    for (int i = 0; i < count; ++i) {
        const float direction = PelletDirection(baseDirection, weapon.spreadRadians, i, count);
        // Muzzle sits at the mount's own shell edge along the firing direction, not its center --
        // a shot spawned dead-center on a hardpoint with any real radius reads as materializing
        // ahead of the ship rather than leaving the gun drawn there.
        const Vec2 muzzle = mountXf.position + FromAngle(direction) * mountRadius;
        const entt::entity projectile = registry.create();
        registry.emplace<WorldTransform>(projectile, muzzle, direction);
        registry.emplace<PreviousTransform>(projectile, muzzle, direction);
        registry.emplace<Velocity>(projectile, FromAngle(direction) * weapon.projectileSpeed, 0.0f);
        registry.emplace<Projectile>(projectile, weapon.damage, weapon.damageType, shooter,
                                     weapon.rangeUnits);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    // exclude<Docked>: a docked rig -- player or NPC -- does not fire (architecture.md 13.3
    // finding H, features.md 3.4's "a docked vessel cannot be shot" made symmetrical).
    for (auto [root, rig, target] : registry.view<Rig, Target>(entt::exclude<Docked>).each()) {
        const bool wantsToFire = registry.all_of<FireIntent>(root);
        const float satisfaction = RigSatisfaction(registry, root);
        const std::optional<Vec2> aimPoint = AimPointPosition(registry, root, target);
        const auto* enabledGroups = registry.try_get<EnabledWeaponGroups>(root);
        const std::uint16_t groupMask = enabledGroups != nullptr ? enabledGroups->mask : 0xFFFFu;

        for (const entt::entity hardpoint : rig.children) {
            auto* weapon = registry.try_get<Weapon>(hardpoint);
            auto* arc = registry.try_get<FiringArc>(hardpoint);
            const auto* mountXf = registry.try_get<WorldTransform>(hardpoint);
            const auto* mountRadius = registry.try_get<HitRadius>(hardpoint);
            // PowerShed (architecture.md 13.3 finding F): a browned-out mount goes offline
            // entirely rather than just cooling down slower -- features.md 2.9's load-shedding
            // is meant to cost hardpoints, not merely fire rate.
            if (weapon == nullptr || arc == nullptr || mountXf == nullptr ||
                registry.any_of<Destroyed, PowerShed>(hardpoint)) {
                continue;
            }

            weapon->cooldown = std::max(0.0f, weapon->cooldown - ctx.dt * satisfaction);

            if (!aimPoint.has_value()) {
                continue;
            }

            const bool onTarget = AimAt(*arc, *mountXf, *aimPoint, ctx.dt);
            const float rangeSq = weapon->rangeUnits * weapon->rangeUnits;
            const bool inRange = DistanceSquared(mountXf->position, *aimPoint) <= rangeSq;

            // Weapon groups (features.md 3.6): a disabled group holds fire, but keeps tracking
            // and cooling down -- it is silenced, not offline like a PowerShed mount above.
            if (wantsToFire && onTarget && inRange && weapon->cooldown <= 0.0f &&
                GroupEnabled(registry, hardpoint, groupMask)) {
                SpawnProjectiles(registry, root, *weapon, *mountXf, *arc,
                                 mountRadius != nullptr ? mountRadius->value : 0.0f);
                weapon->cooldown = weapon->fireIntervalSeconds;
            }
        }
    }

    // One-shot per tick: whatever set it (input/AI) must set it again next tick.
    registry.clear<FireIntent>();
}

}  // namespace sr::space::weapon_system
