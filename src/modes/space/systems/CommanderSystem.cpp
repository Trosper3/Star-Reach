#include "modes/space/systems/CommanderSystem.h"

#include "shared/components/Commander.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"
#include "shared/rig/ModuleAttachment.h"

namespace sr::space::commander_system {
namespace {

// Placeholder, not tuned (see CommanderSystem.h's comment): below this fraction of aggregate
// structural integrity, a commander not already retreating breaks off.
constexpr float kRetreatHealthFraction = 0.25f;

// How far a commander notices a threat around its own vessel, and how far it will pull a
// defender from to answer one. Not tuned -- first-pass placeholders, the same shape
// kRetreatHealthFraction already is.
constexpr float kThreatDetectionRangeUnits = 2500.0f;
constexpr float kDispatchSearchRadiusUnits = 3000.0f;

bool IsHostile(const FactionId& a, const FactionId& b) {
    return !(a == b);
}

// True once `commanderHardpoint`'s own vessel has dropped below the Retreat threshold. A vessel
// with no Rig/Health data at all is treated as fine rather than as destroyed -- the same guard
// NpcAiSystem's own ShouldFlee uses against
// rig_attachment::AggregateStructuralIntegrity's "no data" return of 0.0f, which would otherwise
// read as "already dead."
bool IsBadlyDamaged(const entt::registry& registry, entt::entity commanderHardpoint) {
    const auto* parent = registry.try_get<ParentRig>(commanderHardpoint);
    if (parent == nullptr) {
        return false;
    }
    const auto* rig = registry.try_get<Rig>(parent->root);
    if (rig == nullptr || rig->children.empty()) {
        return false;
    }
    return rig_attachment::AggregateStructuralIntegrity(registry, parent->root) <
           kRetreatHealthFraction;
}

// Nearest hostile, Targetable rig to `position` within `range`. entt::null if nothing qualifies --
// the same shape TargetingSystem::AcquireNearestHostile already uses, duplicated locally rather
// than shared: each system that needs "nearest hostile" keeps its own copy (TargetingSystem,
// CaptureSystem), and this is small enough that a shared helper is not worth the coupling.
entt::entity NearestHostile(const entt::registry& registry, const FactionId& faction,
                            const Vec2& position, float range) {
    entt::entity best = entt::null;
    float bestDistSq = range * range;
    for (auto [candidate, xf, candidateFaction] :
         registry.view<Targetable, WorldTransform, FactionRef>().each()) {
        if (!IsHostile(faction, candidateFaction.id)) {
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

// Nearest same-faction rig to `position`, within `range`, that has no Target of its own yet --
// the "idle" precondition for being dispatched onto someone else's threat instead of sitting in
// Patrol.
entt::entity NearestIdleFriendly(const entt::registry& registry, const FactionId& faction,
                                 const Vec2& position, float range) {
    entt::entity best = entt::null;
    float bestDistSq = range * range;
    for (auto [candidate, target, xf, candidateFaction] :
         registry.view<Target, WorldTransform, FactionRef, Targetable>().each()) {
        if (!(candidateFaction.id == faction) || target.rig != entt::null) {
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

// Commander::orders' first reader: a commander not Retreating that notices a hostile near its own
// vessel dispatches the nearest idle same-faction rig near that threat to engage it directly --
// TargetingSystem's own SensorRange-gated acquisition is a content-starved no-op today (every rig
// authors SensorRange == 0, since no ship mounts a Sensor module), so nothing would organically
// notice a threat without this.
void DispatchDefenders(entt::registry& registry) {
    for (auto [hardpoint, commander] : registry.view<Commander>(entt::exclude<Destroyed>).each()) {
        if (commander.orders == CommanderOrders::Retreat) {
            continue;
        }
        const auto* parent = registry.try_get<ParentRig>(hardpoint);
        if (parent == nullptr) {
            continue;
        }
        const auto* commanderXf = registry.try_get<WorldTransform>(parent->root);
        if (commanderXf == nullptr) {
            continue;
        }

        const entt::entity threat = NearestHostile(
            registry, commander.faction, commanderXf->position, kThreatDetectionRangeUnits);
        if (threat == entt::null) {
            continue;
        }
        const auto* threatXf = registry.try_get<WorldTransform>(threat);
        if (threatXf == nullptr) {
            continue;
        }

        const entt::entity defender = NearestIdleFriendly(
            registry, commander.faction, threatXf->position, kDispatchSearchRadiusUnits);
        if (defender != entt::null) {
            registry.get<Target>(defender).rig = threat;
        }
    }
}

void EscalateRetreat(entt::registry& registry) {
    for (auto [hardpoint, commander] : registry.view<Commander>(entt::exclude<Destroyed>).each()) {
        if (commander.orders != CommanderOrders::Retreat && IsBadlyDamaged(registry, hardpoint)) {
            commander.orders = CommanderOrders::Retreat;
        }
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    EscalateRetreat(registry);
    DispatchDefenders(registry);
}

bool HasLeadership(const entt::registry& registry, const FactionId& faction) {
    for (auto [hardpoint, commander] : registry.view<Commander>(entt::exclude<Destroyed>).each()) {
        if (commander.faction == faction) {
            return true;
        }
    }
    return false;
}

}  // namespace sr::space::commander_system
