#include "modes/space/systems/WeaponSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Health.h"
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
// How close to the cursor's world position counts as "pressed on it" for a chargeToFire+homing
// lock's acquisition step (WeaponStats::chargeToFire's own comment) -- a pick radius rather than
// requiring an exact point-in-shape hit, the same tolerance idea a mouse-picking UI needs for a
// small on-screen target.
constexpr float kLockPickRadius = 40.0f;

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
    return budget != nullptr ? budget->weapons : 1.0f;
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

// A pure function of (hardpoint, pellet index, tick) rather than global RNG state -- Law 2's
// coarse-tick fast-forward needs every time-dependent decision to be reproducible from the same
// inputs. Same FNV-1a shape as MiningSystem::Hash32/RollPercent (MiningSystem.cpp), duplicated
// rather than shared per this codebase's own precedent for a handful of lines like this one.
std::uint32_t Hash32(std::uint32_t seed) {
    std::uint32_t hash = 2166136261u;
    for (int byte = 0; byte < 4; ++byte) {
        hash ^= (seed >> (byte * 8)) & 0xFFu;
        hash *= 16777619u;
    }
    return hash;
}

// WeaponStats::consistency's roll, applied once per pellet: on a hit, `direction` is returned
// unchanged (dead-on); on a miss, it is nudged by a second, independently-salted draw mapped into
// [-accuracyRadians, +accuracyRadians]. Both draws key off `hardpoint` rather than the rig root,
// so two weapons on the same ship firing the same tick do not roll identically.
float ApplyAccuracy(float direction, entt::entity hardpoint, int pelletIndex,
                    unsigned long long tick, float consistency, float accuracyRadians) {
    if (accuracyRadians <= 0.0f || consistency >= 1.0f) {
        return direction;
    }
    const auto base = static_cast<std::uint32_t>(entt::to_integral(hardpoint)) * 2654435761u +
                      static_cast<std::uint32_t>(pelletIndex) * 40503u +
                      static_cast<std::uint32_t>(tick);
    const int percent = std::clamp(static_cast<int>(consistency * 100.0f), 0, 100);
    if (static_cast<int>(Hash32(base) % 100u) < percent) {
        return direction;  // Dead-on roll succeeded.
    }
    // Independently salted (a distinct multiplier before hashing, the same way MiningSystem's own
    // seed keeps elementIndex and tick from colliding) rather than reusing `base` -- otherwise a
    // shot that just failed its accuracy roll would always deviate by the same fixed angle.
    const float t = static_cast<float>(Hash32(base * 2246822519u + 1u) % 100001u) / 100000.0f;
    return direction + accuracyRadians * (2.0f * t - 1.0f);
}

// `arc.currentOffset` is the mount's actual physical bearing this tick (relative to mountXf's own
// rotation) -- AimAt integrates it toward the target but gates firing on it, so using it here
// too is what makes "where the shot goes" match "whether the mount may fire." Recomputing a
// fresh, perfect bearing to aimPoint instead (the previous behaviour) discarded currentOffset
// after AimAt had already spent a tick's worth of traverse computing it (architecture.md 13.3
// finding E).
//
// A locked homing shot (chargeToFire+homing) chases `weapon.lockedTarget` specifically and goes
// permanently dumb if it dies; a plain (non-charge) homing shot carries no target of its own and
// re-seeks the nearest eligible hardpoint every tick instead (ProjectileSystem::Tick).
void SpawnProjectiles(entt::registry& registry, entt::entity shooter, entt::entity hardpoint,
                      const Weapon& weapon, const WorldTransform& mountXf, const FiringArc& arc,
                      unsigned long long tick) {
    const float baseDirection = mountXf.rotation + arc.currentOffset;
    const int count = std::max(weapon.projectilesPerShot, 1);
    const bool homingWasLocked = weapon.chargeToFire && weapon.homingTurnRatePerSecond > 0.0f;
    const entt::entity homingTarget = homingWasLocked ? weapon.lockedTarget : entt::null;

    for (int i = 0; i < count; ++i) {
        float direction = PelletDirection(baseDirection, weapon.spreadRadians, i, count);
        direction = ApplyAccuracy(direction, hardpoint, i, tick, weapon.consistency,
                                  weapon.accuracyRadians);
        // Spawns dead-center on the mount rather than offset toward its shell edge -- WorldRenderer
        // draws a hardpoint as a single circle centered on this same WorldTransform (no muzzle/
        // barrel art to align an edge offset with), so an edge offset only ever misaligned the
        // shot from the drawn hardpoint, reading as firing from whichever side of the circle the
        // offset direction happened to land on rather than from the hardpoint itself.
        const entt::entity projectile = registry.create();
        registry.emplace<WorldTransform>(projectile, mountXf.position, direction);
        registry.emplace<PreviousTransform>(projectile, mountXf.position, direction);
        registry.emplace<Velocity>(projectile, FromAngle(direction) * weapon.projectileSpeed, 0.0f);
        // Designated initializers, not positional: Projectile has grown from four fields to eight
        // since this call was first written, and a long positional list silently mis-assigns every
        // trailing argument the next time it grows (the exact bug class Weapon's own construction
        // in shared/rig/ModuleAttachment.cpp already hit once and switched away from).
        registry.emplace<Projectile>(projectile,
                                     Projectile{
                                         .damage = weapon.damage,
                                         .damageType = weapon.damageType,
                                         .shooter = shooter,
                                         .remainingRange = weapon.rangeUnits,
                                         .homingTurnRatePerSecond = weapon.homingTurnRatePerSecond,
                                         .homingTarget = homingTarget,
                                         .homingWasLocked = homingWasLocked,
                                         .colorOverride = weapon.colorOverride,
                                     });
    }
}

// Nearest hardpoint (by HitRadius + WorldTransform) to `point`, excluding `excludeRoot`'s own rig
// and anything already destroyed, no farther than `maxDistance`. entt::null if nothing qualifies.
// Two callers: a chargeToFire+homing lock's acquisition step (point = the cursor, maxDistance =
// kLockPickRadius) and a plain continuous+homing weapon's automatic seek (point = the mount
// itself, maxDistance = weapon range). Same shape as ProjectileSystem::FindHit
// (ProjectileSystem.cpp), duplicated here rather than shared: FindHit is private to its own file
// for the same "small query, one caller's neighborhood" reason this one is.
entt::entity FindNearestHittable(const entt::registry& registry, Vec2 point,
                                 entt::entity excludeRoot, float maxDistance) {
    entt::entity best = entt::null;
    float bestDistSq = maxDistance * maxDistance;
    for (auto [hardpoint, hitRadius, hpXf] : registry.view<HitRadius, WorldTransform>().each()) {
        (void)hitRadius;
        const auto* parent = registry.try_get<ParentRig>(hardpoint);
        if ((parent != nullptr && parent->root == excludeRoot) ||
            registry.all_of<Destroyed>(hardpoint)) {
            continue;
        }
        const float distSq = DistanceSquared(point, hpXf.position);
        if (distSq <= bestDistSq) {
            best = hardpoint;
            bestDistSq = distSq;
        }
    }
    return best;
}

// Beam hit-test: nearest hardpoint whose HitRadius the segment [from, to] crosses, most-specific-
// wins (smallest radius, ties broken by nearest-along-the-segment), excluding the shooter's own
// rig and anything already destroyed -- the same query ProjectileSystem::FindHit runs against a
// moving projectile, run here once per tick against a fixed segment instead, since a beam has no
// travel time. Duplicated locally for the same reason FindNearestHittable above is: FindHit is
// private to ProjectileSystem.cpp. Writes the segment point closest to whatever it hit (or `to`,
// if nothing did) to `outHitPoint`, which is what WorldRenderer draws the beam line out to.
entt::entity FindBeamHit(const entt::registry& registry, entt::entity shooter, Vec2 from, Vec2 to,
                         Vec2& outHitPoint) {
    entt::entity best = entt::null;
    float bestRadius = 0.0f;
    float bestT = 0.0f;
    const Vec2 segment = to - from;
    const float lengthSq = LengthSquared(segment);
    outHitPoint = to;

    for (auto [hardpoint, hitRadius, hpXf] : registry.view<HitRadius, WorldTransform>().each()) {
        const auto* parent = registry.try_get<ParentRig>(hardpoint);
        if ((parent != nullptr && parent->root == shooter) ||
            registry.all_of<Destroyed>(hardpoint)) {
            continue;
        }
        const float t = lengthSq > 0.0f
                            ? std::clamp(Dot(hpXf.position - from, segment) / lengthSq, 0.0f, 1.0f)
                            : 0.0f;
        const Vec2 closest = from + segment * t;
        const float distance = Distance(hpXf.position, closest);
        if (distance > hitRadius.value) {
            continue;
        }
        if (best == entt::null || hitRadius.value < bestRadius ||
            (hitRadius.value == bestRadius && t < bestT)) {
            best = hardpoint;
            bestRadius = hitRadius.value;
            bestT = t;
            outHitPoint = closest;
        }
    }
    return best;
}

// Accumulates rather than overwrites, the same rule ProjectileSystem's own QueueDamage follows:
// a beam landing on a hardpoint another shot already queued damage on this tick must not lose
// either.
void QueueDamage(entt::registry& registry, entt::entity hardpoint, float amount, DamageType type,
                 entt::entity source) {
    if (auto* pending = registry.try_get<PendingDamage>(hardpoint)) {
        pending->amount += amount;
        pending->type = type;
        pending->source = source;
    } else {
        registry.emplace<PendingDamage>(hardpoint, amount, type, source);
    }
}

// Runs one tick of an in-progress burst (WeaponStats::burstCount's own comment): decrements the
// timer, fires the next shot when it elapses, and closes the burst out (sets the normal cooldown)
// on its last shot or if ammo runs dry. Returns true if the hardpoint was mid-burst this tick --
// the caller should not also run the normal charge/fire logic below when this is true. Never true
// for a continuous weapon: its firing branch (FireContinuous below) never touches
// burstShotsRemaining.
bool TickBurst(entt::registry& registry, entt::entity root, entt::entity hardpoint, Weapon& weapon,
               const WorldTransform& mountXf, const FiringArc& arc, float dt, float satisfaction,
               unsigned long long tick) {
    if (weapon.burstShotsRemaining <= 0) {
        return false;
    }
    weapon.burstTimer -= dt * satisfaction;
    if (weapon.burstTimer <= 0.0f) {
        const bool outOfAmmo = weapon.maxAmmo >= 0.0f && weapon.ammoRemaining <= 0.0f;
        if (outOfAmmo) {
            weapon.burstShotsRemaining = 0;
            weapon.cooldown = weapon.fireIntervalSeconds;
        } else {
            SpawnProjectiles(registry, root, hardpoint, weapon, mountXf, arc, tick);
            if (weapon.maxAmmo >= 0.0f) {
                weapon.ammoRemaining -= 1.0f;
            }
            weapon.burstShotsRemaining -= 1;
            weapon.burstTimer = weapon.burstIntervalSeconds;
            if (weapon.burstShotsRemaining <= 0) {
                weapon.cooldown = weapon.fireIntervalSeconds;
            }
        }
    }
    return true;
}

// Resolves this tick's effective aim point and, for a chargeToFire+homing weapon, this hold's
// lock acquisition/maintenance -- decided once, on the tick the hold begins, from whatever is
// under the cursor right then, not re-picked every tick, and holding does not require keeping the
// cursor on it afterward. Returns nullopt if a hold just started with nothing under the cursor, or
// if an established lock died mid-charge: either way, this hold never fires, and the caller resets
// nothing itself -- Weapon::chargeSeconds/lockedTarget are already cleared here on the losing path.
//
// A held lock always wins and auto-tracks regardless of the cursor; a plain (non-charge)
// continuous+homing weapon re-seeks whatever is nearest every tick instead; everything else aims
// at the cursor, same as always (features.md 3.2). Dumb-fire discrete homing (no charge) is
// deliberately absent here -- it aims at the cursor like any other weapon and only starts curving
// after ProjectileSystem advances it, never before it is fired.
std::optional<Vec2> ResolveAimPoint(entt::registry& registry, entt::entity root, Weapon& weapon,
                                    const WorldTransform& mountXf, const Vec2& cursor) {
    const bool homingLock = weapon.chargeToFire && weapon.homingTurnRatePerSecond > 0.0f;
    if (homingLock) {
        if (weapon.chargeSeconds == 0.0f) {
            const entt::entity candidate =
                FindNearestHittable(registry, cursor, root, kLockPickRadius);
            if (candidate == entt::null) {
                return std::nullopt;  // Nothing under the cursor -- this hold never charges.
            }
            weapon.lockedTarget = candidate;
        } else if (weapon.lockedTarget == entt::null || !registry.valid(weapon.lockedTarget) ||
                   registry.all_of<Destroyed>(weapon.lockedTarget)) {
            weapon.chargeSeconds = 0.0f;
            weapon.lockedTarget = entt::null;
            return std::nullopt;  // Lock broken mid-charge.
        }
        return registry.get<WorldTransform>(weapon.lockedTarget).position;
    }
    if (weapon.continuous && weapon.homingTurnRatePerSecond > 0.0f) {
        const entt::entity nearest =
            FindNearestHittable(registry, mountXf.position, root, weapon.rangeUnits);
        if (nearest != entt::null) {
            return registry.get<WorldTransform>(nearest).position;
        }
    }
    return cursor;
}

// Fires one tick of continuous (beam) damage: hit-tests the current aim direction out to
// rangeUnits, queues PendingDamage scaled by dt and power satisfaction, and leaves BeamState for
// WorldRenderer to draw. No cooldown and no per-shot ammo -- WeaponStats::continuous's own
// comment covers why fireIntervalSeconds/projectileSpeed/spreadRadians/projectilesPerShot are all
// meaningless here.
void FireContinuous(entt::registry& registry, entt::entity root, entt::entity hardpoint,
                    Weapon& weapon, const WorldTransform& mountXf, const FiringArc& arc, float dt,
                    float satisfaction) {
    const float direction = mountXf.rotation + arc.currentOffset;
    const Vec2 to = mountXf.position + FromAngle(direction) * weapon.rangeUnits;
    Vec2 hitPoint = to;
    const entt::entity hit = FindBeamHit(registry, root, mountXf.position, to, hitPoint);
    if (hit != entt::null) {
        QueueDamage(registry, hit, weapon.damage * dt * satisfaction, weapon.damageType, root);
    }
    registry.emplace_or_replace<BeamState>(hardpoint,
                                           BeamState{.endPoint = hitPoint,
                                                     .damageType = weapon.damageType,
                                                     .colorOverride = weapon.colorOverride});
    if (weapon.maxAmmo >= 0.0f) {
        // 1.0 per second of continuous fire (WeaponStats::maxAmmo's own comment).
        weapon.ammoRemaining = std::max(0.0f, weapon.ammoRemaining - dt);
    }
}

// Charge gate plus every other must-hold condition, combined: no separate "release to fire" path
// exists (WeaponStats::chargeToFire's own comment) -- once charge is complete while still held,
// the weapon may fire, be it a discrete shot right then or a beam that simply starts.
bool ReadyToFire(const entt::registry& registry, entt::entity hardpoint, Weapon& weapon,
                 bool onTarget, bool inRange, std::uint16_t groupMask, float dt,
                 float satisfaction) {
    bool charged = true;
    if (weapon.chargeToFire) {
        weapon.chargeSeconds =
            std::min(weapon.chargeSecondsToFire, weapon.chargeSeconds + dt * satisfaction);
        charged = weapon.chargeSeconds >= weapon.chargeSecondsToFire;
    }
    const bool outOfAmmo = weapon.maxAmmo >= 0.0f && weapon.ammoRemaining <= 0.0f;
    // Weapon groups (features.md 3.6): a disabled group holds fire, but keeps tracking and
    // cooling down -- it is silenced, not offline like PowerShed (Tick's own comment).
    return charged && onTarget && inRange && weapon.cooldown <= 0.0f && !outOfAmmo &&
           GroupEnabled(registry, hardpoint, groupMask);
}

// Fires one discrete shot (or, for burstCount > 1, kicks off the burst TickBurst above plays out
// on its own timer) and consumes one unit of ammo if the weapon is ammo-limited.
void FireDiscrete(entt::registry& registry, entt::entity root, entt::entity hardpoint,
                  Weapon& weapon, const WorldTransform& mountXf, const FiringArc& arc,
                  unsigned long long tick) {
    SpawnProjectiles(registry, root, hardpoint, weapon, mountXf, arc, tick);
    if (weapon.maxAmmo >= 0.0f) {
        weapon.ammoRemaining -= 1.0f;
    }
    if (weapon.burstCount > 1) {
        weapon.burstShotsRemaining = weapon.burstCount - 1;
        weapon.burstTimer = weapon.burstIntervalSeconds;
        // Cooldown withheld until the whole burst completes -- TickBurst sets it on the final shot.
    } else {
        weapon.cooldown = weapon.fireIntervalSeconds;
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
            // Cleared unconditionally, every tick, for every hardpoint -- cheap (a no-op sparse-
            // set check on the vast majority that never had one), and correct regardless of which
            // `continue` below ends up firing: a beam re-establishes BeamState only by actually
            // firing this same tick, a few lines down, so nothing below has to remember to clear
            // it on every early-exit path individually.
            registry.remove<BeamState>(hardpoint);

            auto* weapon = registry.try_get<Weapon>(hardpoint);
            auto* arc = registry.try_get<FiringArc>(hardpoint);
            const auto* mountXf = registry.try_get<WorldTransform>(hardpoint);
            // PowerShed (architecture.md 13.3 finding F): a browned-out mount goes offline
            // entirely rather than just cooling down slower -- features.md 2.9's load-shedding
            // is meant to cost hardpoints, not merely fire rate.
            if (weapon == nullptr || arc == nullptr || mountXf == nullptr ||
                registry.any_of<Destroyed, PowerShed>(hardpoint)) {
                continue;
            }

            weapon->cooldown = std::max(0.0f, weapon->cooldown - ctx.dt * satisfaction);

            if (TickBurst(registry, root, hardpoint, *weapon, *mountXf, *arc, ctx.dt, satisfaction,
                          ctx.tick)) {
                continue;
            }

            // A released trigger always resets charge/lock, whatever else is going on -- a charge
            // never survives a release (WeaponStats::chargeToFire's own comment).
            if (!wantsToFire) {
                weapon->chargeSeconds = 0.0f;
                weapon->lockedTarget = entt::null;
                continue;
            }
            if (!aimPoint.has_value()) {
                continue;
            }

            const std::optional<Vec2> effectiveAimPoint =
                ResolveAimPoint(registry, root, *weapon, *mountXf, *aimPoint);
            if (!effectiveAimPoint.has_value()) {
                continue;
            }

            const bool onTarget = AimAt(*arc, *mountXf, *effectiveAimPoint, ctx.dt);
            const float rangeSq = weapon->rangeUnits * weapon->rangeUnits;
            const bool inRange = DistanceSquared(mountXf->position, *effectiveAimPoint) <= rangeSq;

            if (!ReadyToFire(registry, hardpoint, *weapon, onTarget, inRange, groupMask, ctx.dt,
                             satisfaction)) {
                continue;
            }

            if (weapon->continuous) {
                FireContinuous(registry, root, hardpoint, *weapon, *mountXf, *arc, ctx.dt,
                               satisfaction);
            } else {
                FireDiscrete(registry, root, hardpoint, *weapon, *mountXf, *arc, ctx.tick);
            }
        }
    }

    // One-shot per tick: whatever set it (input/AI) must set it again next tick.
    registry.clear<FireIntent>();
}

}  // namespace sr::space::weapon_system
