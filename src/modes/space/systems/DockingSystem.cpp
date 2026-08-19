#include "modes/space/systems/DockingSystem.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "shared/components/Docking.h"
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

// Nearest living DockingBay hardpoint belonging to a different, same-faction rig, within range.
// entt::null if nothing qualifies. exclude<Destroyed>: a destroyed bay still carries DockingBay/
// ParentRig/WorldTransform (DamageSystem only tags Destroyed, never strips components), so
// without this a wrecked bay kept prompting "[R] DOCK" and would actually dock the player onto
// it -- found during #133's M1 verification pass.
entt::entity FindEligibleBay(const entt::registry& registry, entt::entity self,
                             const FactionId& faction, const Vec2& position, float maxRange) {
    entt::entity best = entt::null;
    float bestDist = std::numeric_limits<float>::max();
    for (auto [bay, parent, bayXf] :
         registry.view<DockingBay, ParentRig, WorldTransform>(entt::exclude<Destroyed>).each()) {
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

// architecture.md 13.3 finding I / 13.4 decision 1: docking no longer heals for free -- that
// belonged to StationServicesSystem's facility-gated, paid Repair path (P0-11), and running both
// made the paid path unsellable. This function keeps every other part of "being docked": staying
// put, and clearing on an UndockRequest.
void ImmobilizeDocked(entt::registry& registry) {
    std::vector<entt::entity> toUndock;

    for (auto [self, docked] : registry.view<Docked>().each()) {
        (void)docked;

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
    ImmobilizeDocked(registry);
}

}  // namespace sr::space::docking_system
