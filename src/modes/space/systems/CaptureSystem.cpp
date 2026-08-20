#include "modes/space/systems/CaptureSystem.h"

#include <utility>
#include <vector>

#include "shared/components/Capture.h"
#include "shared/components/Commander.h"
#include "shared/components/Docking.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"

namespace sr::space::capture_system {
namespace {

// Boarding range and hold duration are first-pass placeholders, the same "not tuned" shape
// CommanderSystem's own kRetreatHealthFraction carries -- features.md 3.2's Troop Bay is meant to
// scale the hold by capacity vs. the target's hull class, but that module does not exist yet
// (CaptureSystem.h), so every boarder pays the same flat cost until it lands.
constexpr float kBoardingRangeUnits = 150.0f;
constexpr float kBoardingHoldSeconds = 10.0f;

bool IsHostile(const FactionRef& a, const FactionRef& b) {
    return !(a.id == b.id);
}

// Nearest Uncrewed, Targetable, hostile rig within boarding range of `position`. entt::null if
// nothing qualifies.
//
// Requiring Targetable rather than a separate exclude<Destroyed> matches TargetingSystem's own
// IsValidTarget: DamageSystem's ApplyStructuralFailure and CascadeDockedDestruction both remove
// Targetable in the same tick they emplace Destroyed, so a wrecked or docked hull is already
// excluded for free. A crewed hull never carries Uncrewed at all (ModuleAttachment.cpp's
// RecomputeRigTotals), so "a crewed hull cannot be captured" holds by construction -- there is no
// separate crewed/uncrewed branch to get wrong here.
entt::entity FindBoardableTarget(const entt::registry& registry, entt::entity self,
                                 const FactionRef& boarderFaction, const Vec2& position) {
    entt::entity best = entt::null;
    float bestDistSq = kBoardingRangeUnits * kBoardingRangeUnits;

    for (auto [candidate, xf, faction] :
         registry.view<Uncrewed, Targetable, WorldTransform, FactionRef>().each()) {
        if (candidate == self || !IsHostile(boarderFaction, faction)) {
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

// Flips `target`'s ownership to `newFaction`. A Commander aboard is the one thing that needs its
// own field touched: its `network` id stays put (features.md 2.5 anchors a knowledge network to
// the commander entity, not the hull), but `faction` must follow the hull, or a captured
// sub-commander would keep fighting for its old side from inside a ship that now belongs to
// someone else. Every other surviving hardpoint -- ordinary crew included -- carries no FactionRef
// of its own, so it transfers for free.
void TransferOwnership(entt::registry& registry, entt::entity target, const FactionId& newFaction) {
    registry.get<FactionRef>(target).id = newFaction;
    if (auto* commander = registry.try_get<Commander>(target)) {
        commander->faction = newFaction;
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<std::pair<entt::entity, entt::entity>> completed;  // (boarder, target)

    // exclude<Docked>: a docked rig cannot also be boarding something. exclude<Uncrewed>: a
    // derelict hull has no crew of its own left to send across.
    for (auto [self, xf, faction] :
         registry.view<WorldTransform, FactionRef, Targetable>(entt::exclude<Docked, Uncrewed>)
             .each()) {
        const entt::entity target = FindBoardableTarget(registry, self, faction, xf.position);

        if (target == entt::null) {
            registry.remove<BoardingHold>(self);
            continue;
        }

        auto* hold = registry.try_get<BoardingHold>(self);
        if (hold == nullptr || hold->target != target) {
            registry.emplace_or_replace<BoardingHold>(self, BoardingHold{target, 0.0f});
            continue;
        }

        hold->heldSeconds += ctx.dt;
        if (hold->heldSeconds >= kBoardingHoldSeconds) {
            completed.emplace_back(self, target);
        }
    }

    for (const auto& [boarder, target] : completed) {
        TransferOwnership(registry, target, registry.get<FactionRef>(boarder).id);
        registry.remove<BoardingHold>(boarder);
    }
}

}  // namespace sr::space::capture_system
