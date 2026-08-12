#include "modes/space/systems/ProjectileSystem.h"

#include <algorithm>
#include <vector>

#include "shared/components/Combat.h"
#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"

namespace sr::space::projectile_system {
namespace {

// Closest-approach distance from `point` to the segment [from, to]. Used instead of an
// end-of-step point check because projectile speeds are routinely comparable to hardpoint radii
// over one fixed tick (900+ units/s at 60 Hz is ~15 units per step, against hardpoint radii of
// 7-22) -- a point check alone would tunnel through anything it did not land exactly on.
float DistanceToSegment(const Vec2& point, const Vec2& from, const Vec2& to) {
    const Vec2 segment = to - from;
    const float lengthSq = LengthSquared(segment);
    if (lengthSq <= 0.0f) {
        return Distance(point, from);
    }
    const float t = std::clamp(Dot(point - from, segment) / lengthSq, 0.0f, 1.0f);
    return Distance(point, from + segment * t);
}

// Nearest-in-iteration-order thing a shot can hit -- a hardpoint OR an asteroid, uniformly,
// whose hit radius the segment [from, to] crosses -- skipping the shooter's own rig (Combat.h:
// Projectile::shooter "used to skip self-hits") and anything already destroyed this tick.
// entt::null if the path is clear.
//
// ParentRig is looked up in the body rather than named in the view: it is only there to find the
// shooter's own rig, and narrowing the view to it would silently exclude every entity with
// HitRadius but no ParentRig -- asteroids, chiefly, which is exactly what made them unshootable.
entt::entity FindHit(const entt::registry& registry, const Projectile& projectile, const Vec2& from,
                     const Vec2& to) {
    for (auto [hardpoint, hitRadius, hpXf] : registry.view<HitRadius, WorldTransform>().each()) {
        const auto* parent = registry.try_get<ParentRig>(hardpoint);
        if ((parent != nullptr && parent->root == projectile.shooter) ||
            registry.all_of<Destroyed>(hardpoint)) {
            continue;
        }
        if (DistanceToSegment(hpXf.position, from, to) <= hitRadius.value) {
            return hardpoint;
        }
    }
    return entt::null;
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
