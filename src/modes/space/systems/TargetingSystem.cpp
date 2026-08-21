#include "modes/space/systems/TargetingSystem.h"

#include <limits>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"

namespace sr::space::targeting_system {

namespace {

// True when `seeker`'s stance toward `other` is Hostile or War -- the two bands features.md 5.3
// describes as "fired on" (in claimed territory, or anywhere, respectively). No territory model
// exists yet (architecture.md 12.32), so both collapse to one auto-acquire threshold instead of
// distinguishing them. A null matrix fails closed -- nothing is hostile until ctx.diplomacy is
// wired (architecture.md 12.24 step 6) -- the same convention TemplateMarketSystem::PassesGate
// uses for the mirror-image gate.
bool IsHostile(const core::diplomacy::DiplomacyMatrix* diplomacy, const FactionRef& seeker,
              const FactionRef& other) {
    if (diplomacy == nullptr) {
        return false;
    }
    return diplomacy->Get(seeker.id, other.id) <= core::diplomacy::Relation::Hostile;
}

// True while `rig` is still a legal target for `seekerFaction`: alive, Targetable, and hostile.
// A rig that stopped being any of those needs to be dropped THIS tick, or the seeker spends the
// tick aiming at nothing.
bool IsValidTarget(const entt::registry& registry, entt::entity rig,
                   const core::diplomacy::DiplomacyMatrix* diplomacy,
                   const FactionRef& seekerFaction) {
    if (rig == entt::null || !registry.valid(rig)) {
        return false;
    }
    if (!registry.all_of<Targetable, FactionRef>(rig)) {
        return false;
    }
    return IsHostile(diplomacy, seekerFaction, registry.get<FactionRef>(rig));
}

// Nearest hostile, Targetable rig within `range` of `position`. entt::null if nothing qualifies.
entt::entity AcquireNearestHostile(const entt::registry& registry, entt::entity self,
                                   const Vec2& position,
                                   const core::diplomacy::DiplomacyMatrix* diplomacy,
                                   const FactionRef& seekerFaction, float range) {
    entt::entity best = entt::null;
    float bestDistSq = range * range;

    for (auto [candidate, xf, faction] :
         registry.view<Targetable, WorldTransform, FactionRef>().each()) {
        if (candidate == self || !IsHostile(diplomacy, seekerFaction, faction)) {
            continue;
        }
        const float distSq = DistanceSquared(position, xf.position);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = candidate;
        }
    }
    return best;
}

// Nearest living (non-Destroyed) hardpoint of `rig` to `position` -- the aim point. entt::null if
// the rig has no positioned hardpoints left; callers keep the rig target regardless, since
// DamageSystem, not this one, decides whether a hardpoint-less rig still exists.
entt::entity SelectAimPoint(const entt::registry& registry, entt::entity rig,
                            const Vec2& position) {
    const auto* rigChildren = registry.try_get<Rig>(rig);
    if (rigChildren == nullptr) {
        return entt::null;
    }

    entt::entity best = entt::null;
    float bestDistSq = std::numeric_limits<float>::max();
    for (const entt::entity hardpoint : rigChildren->children) {
        if (!registry.valid(hardpoint) || registry.all_of<Destroyed>(hardpoint)) {
            continue;
        }
        const auto* xf = registry.try_get<WorldTransform>(hardpoint);
        if (xf == nullptr) {
            continue;
        }
        const float distSq = DistanceSquared(position, xf->position);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = hardpoint;
        }
    }
    return best;
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    // exclude<PlayerControlled>: features.md 3.2 forbids the player an automatic lock -- the
    // player aims manually via AimPoint (WeaponSystem), so this system must never acquire or
    // hold a Target for them, the same predicate NpcAiSystem already uses for the same reason.
    for (auto [seeker, target, xf, faction, sensor] :
         registry
             .view<Target, WorldTransform, FactionRef, SensorRange>(entt::exclude<PlayerControlled>)
             .each()) {
        if (!IsValidTarget(registry, target.rig, ctx.diplomacy, faction)) {
            target.rig = entt::null;
            target.hardpoint = entt::null;
        }

        if (target.rig == entt::null) {
            target.rig = AcquireNearestHostile(registry, seeker, xf.position, ctx.diplomacy,
                                               faction, sensor.units);
        }

        if (target.rig == entt::null) {
            continue;
        }

        // The aim point survives from a prior tick unless it died -- that persistence is the
        // whole reason Target carries a separate hardpoint field (Targeting.h).
        if (target.hardpoint == entt::null || !registry.valid(target.hardpoint) ||
            registry.all_of<Destroyed>(target.hardpoint)) {
            target.hardpoint = SelectAimPoint(registry, target.rig, xf.position);
        }
    }
}

}  // namespace sr::space::targeting_system
