#include "modes/space/systems/DockingSystem.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "shared/components/Docking.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

namespace sr::space::docking_system {
namespace {

// Distance from a docking-bay hardpoint at which a rig is considered "arrived" -- ported
// verbatim from legacy StarReach2's DockRepair.h kDockRadius.
constexpr float kDockRangeUnits = 50.0f;

// Fraction of a hardpoint's max hull restored per second while docked -- ported verbatim from
// legacy StarReach2's DockRepair.h kDockHealPerSecond (~6.7s to fully heal).
constexpr float kDockHealPerSecond = 0.15f;

// Nearest DockingBay hardpoint belonging to a different, same-faction rig, within range.
// entt::null if nothing qualifies.
entt::entity FindEligibleBay(const entt::registry& registry, entt::entity self,
                             const FactionId& faction, const Vec2& position, float maxRange) {
    entt::entity best = entt::null;
    float bestDist = std::numeric_limits<float>::max();
    for (auto [bay, parent, bayXf] :
         registry.view<DockingBay, ParentRig, WorldTransform>().each()) {
        if (parent.root == self) {
            continue;  // A rig cannot dock with its own bay.
        }
        const auto* stationFaction = registry.try_get<FactionRef>(parent.root);
        if (stationFaction == nullptr || !(stationFaction->id == faction)) {
            continue;
        }
        const float dist = Distance(position, bayXf.position);
        if (dist <= maxRange && dist < bestDist) {
            bestDist = dist;
            best = bay;
        }
    }
    return best;
}

void UpdatePromptsAndRequests(entt::registry& registry) {
    std::vector<std::pair<entt::entity, entt::entity>> toDock;  // (rig root, bay)

    for (auto [self, xf, faction] :
         registry.view<WorldTransform, FactionRef, Targetable>(entt::exclude<Docked>).each()) {
        const entt::entity nearestBay =
            FindEligibleBay(registry, self, faction.id, xf.position, kDockRangeUnits);

        if (nearestBay != entt::null) {
            registry.emplace_or_replace<DockPrompt>(self, nearestBay);
        } else {
            registry.remove<DockPrompt>(self);
        }

        if (const auto* request = registry.try_get<DockRequest>(self)) {
            if (nearestBay != entt::null && request->bay == nearestBay) {
                toDock.emplace_back(self, nearestBay);
            }
            registry.remove<DockRequest>(self);
        }
    }

    for (const auto& [self, bay] : toDock) {
        const entt::entity station = registry.get<ParentRig>(bay).root;
        registry.emplace<Docked>(self, station, bay);
        registry.remove<Targetable>(self);
        registry.remove<DockPrompt>(self);
    }
}

void HealAndImmobilize(entt::registry& registry, float dt) {
    std::vector<entt::entity> toUndock;

    for (auto [self, docked, rig] : registry.view<Docked, Rig>().each()) {
        (void)docked;
        for (const entt::entity child : rig.children) {
            if (registry.all_of<Destroyed>(child)) {
                continue;  // Destruction is permanent (Health.h) -- docking never revives one.
            }
            if (auto* health = registry.try_get<Health>(child)) {
                health->current =
                    std::min(health->max, health->current + kDockHealPerSecond * health->max * dt);
            }
        }

        if (auto* velocity = registry.try_get<Velocity>(self)) {
            *velocity = Velocity{};
        }
        if (auto* thrust = registry.try_get<ThrustInput>(self)) {
            *thrust = ThrustInput{};
        }

        if (registry.all_of<UndockRequest>(self)) {
            toUndock.push_back(self);
        }
    }

    for (const entt::entity self : toUndock) {
        registry.remove<Docked>(self);
        registry.remove<UndockRequest>(self);
        registry.emplace_or_replace<Targetable>(self);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    UpdatePromptsAndRequests(registry);
    HealAndImmobilize(registry, ctx.dt);
}

}  // namespace sr::space::docking_system
