#include "modes/space/systems/ProjectileSystem.h"

#include <algorithm>
#include <vector>

#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"

namespace sr::space::projectile_system {
namespace {

// Closest-approach point on the segment [from, to] to `point`, as a fraction 0..1 along it, plus
// the resulting distance. Used instead of an end-of-step point check because projectile speeds
// are routinely comparable to hardpoint radii over one fixed tick (900+ units/s at 60 Hz is ~15
// units per step, against hardpoint radii of 7-22) -- a point check alone would tunnel through
// anything it did not land exactly on. `t` is also most-specific-wins' tie-break key below: two
// hardpoints of equal radius resolve to whichever the segment reaches first.
struct SegmentProjection {
    float t;
    float distance;
};

SegmentProjection ProjectOntoSegment(const Vec2& point, const Vec2& from, const Vec2& to) {
    const Vec2 segment = to - from;
    const float lengthSq = LengthSquared(segment);
    if (lengthSq <= 0.0f) {
        return SegmentProjection{0.0f, Distance(point, from)};
    }
    const float t = std::clamp(Dot(point - from, segment) / lengthSq, 0.0f, 1.0f);
    return SegmentProjection{t, Distance(point, from + segment * t)};
}

// Most-specific-wins (features.md 3.5, replacing the old first-in-EnTT-iteration-order pick):
// among every hardpoint OR asteroid, uniformly, whose hit radius the segment [from, to] crosses,
// the one with the SMALLEST hitRadius wins -- a shot that also crosses the chassis behind a
// turret still hits the turret, since the chassis is the structural backstop, not a fallback
// target. Ties (equal radius) resolve to whichever the segment reaches first, so the outcome
// never depends on EnTT's iteration order. Skips the shooter's own rig (Combat.h:
// Projectile::shooter "used to skip self-hits"), anything already destroyed this tick, and a
// docked rig's hardpoints (features.md 3.4's "a docked vessel is not a target," the exclusion
// half architecture.md 12.34 specifies). entt::null if the path is clear.
//
// ParentRig is looked up in the body rather than named in the view: it is only there to find the
// shooter's own rig and the docked state of the rig a hardpoint belongs to, and narrowing the
// view to it would silently exclude every entity with HitRadius but no ParentRig -- asteroids,
// chiefly, which is exactly what made them unshootable. Docked itself is not view-level
// exclude<Docked> either, for the same reason: it lives on the rig root, and this view is over
// hardpoints, which never carry it.
entt::entity FindHit(const entt::registry& registry, const Projectile& projectile, const Vec2& from,
                     const Vec2& to) {
    entt::entity best = entt::null;
    float bestRadius = 0.0f;
    float bestT = 0.0f;

    for (auto [hardpoint, hitRadius, hpXf] : registry.view<HitRadius, WorldTransform>().each()) {
        const auto* parent = registry.try_get<ParentRig>(hardpoint);
        const bool parentDocked = parent != nullptr && registry.all_of<Docked>(parent->root);
        if ((parent != nullptr && parent->root == projectile.shooter) ||
            registry.all_of<Destroyed>(hardpoint) || parentDocked) {
            continue;
        }
        const SegmentProjection projection = ProjectOntoSegment(hpXf.position, from, to);
        if (projection.distance > hitRadius.value) {
            continue;
        }
        if (best == entt::null || hitRadius.value < bestRadius ||
            (hitRadius.value == bestRadius && projection.t < bestT)) {
            best = hardpoint;
            bestRadius = hitRadius.value;
            bestT = projection.t;
        }
    }
    return best;
}

// Accumulates rather than overwrites: two projectiles landing on the same hardpoint in the same
// tick must not lose one to the other. Mixed damage types in one tick collapse to the latest
// hit's type -- PendingDamage has one type slot, not a per-type breakdown.
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

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> toDestroy;

    for (auto [entity, xf, prev, velocity, projectile] :
         registry.view<WorldTransform, PreviousTransform, Velocity, Projectile>().each()) {
        const Vec2 oldPos = xf.position;
        const Vec2 step = velocity.linear * ctx.dt;
        const Vec2 newPos = oldPos + step;

        prev = PreviousTransform{oldPos, xf.rotation};
        xf.position = newPos;
        projectile.remainingRange -= Length(step);

        const entt::entity hit = FindHit(registry, projectile, oldPos, newPos);
        if (hit != entt::null) {
            QueueDamage(registry, hit, projectile.damage, projectile.damageType,
                        projectile.shooter);
            toDestroy.push_back(entity);
        } else if (projectile.remainingRange <= 0.0f) {
            toDestroy.push_back(entity);
        }
    }

    for (const entt::entity entity : toDestroy) {
        registry.destroy(entity);
    }
}

}  // namespace sr::space::projectile_system
