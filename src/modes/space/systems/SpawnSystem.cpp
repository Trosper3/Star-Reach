#include "modes/space/systems/SpawnSystem.h"

#include <limits>
#include <vector>

#include "shared/components/Docking.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Spawn.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"
#include "shared/math/Vec2.h"

namespace sr::space::spawn_system {
namespace {

// Beyond this distance from every SpawnAnchor, a rig is doing nothing anyone can see or reach --
// there is no Tier 2/3 registry to hand it off to yet (architecture.md Law 2), so the alternative
// is an ever-growing registry of ships nobody is simulating meaningfully.
constexpr float kCullRadiusUnits = 20000.0f;

// Ring search for a placement: try increasingly distant rings, evenly-spaced angles per ring, and
// take the first spot clear of every other rig's CollisionRadius. Ported from legacy StarReach2's
// GetSafeSpawnPosition, which retried the SAME radius band up to 100 times and gave up outright
// if the map was "swamped" -- expanding the radius each pass means a crowded anchor still
// resolves to the nearest ring with room, instead of failing the same band forever.
constexpr int kRingCount = 8;
constexpr int kAttemptsPerRing = 12;
constexpr float kRingSpacingUnits = 150.0f;

// Distance from `position` to the nearest SpawnAnchor, and which one. Returns false with both
// output params untouched if the registry has no SpawnAnchor at all -- callers must treat that as
// "nothing to measure against" rather than as "infinitely far".
bool FindNearestAnchor(const entt::registry& registry, const Vec2& position,
                       entt::entity& outAnchor, float& outDistance) {
    bool found = false;
    float best = std::numeric_limits<float>::max();
    entt::entity bestAnchor = entt::null;
    for (auto [anchor, anchorXf] : registry.view<SpawnAnchor, WorldTransform>().each()) {
        const float dist = Distance(position, anchorXf.position);
        if (dist < best) {
            best = dist;
            bestAnchor = anchor;
            found = true;
        }
    }
    if (found) {
        outAnchor = bestAnchor;
        outDistance = best;
    }
    return found;
}

// True if `candidate` sits at least `margin` away from every other living rig's CollisionRadius,
// so relocating `self` here does not drop it inside another ship's hull.
bool IsClear(const entt::registry& registry, entt::entity self, const Vec2& candidate,
             float margin) {
    for (auto [other, otherXf, radius] :
         registry.view<WorldTransform, CollisionRadius>(entt::exclude<Destroyed>).each()) {
        if (other == self) {
            continue;
        }
        if (Distance(candidate, otherXf.position) < radius.value + margin) {
            return false;
        }
    }
    return true;
}

Vec2 FindSafePlacement(const entt::registry& registry, entt::entity self, const Vec2& anchorPos,
                       float margin) {
    Vec2 fallback = anchorPos;
    for (int ring = 1; ring <= kRingCount; ++ring) {
        const float ringRadius = static_cast<float>(ring) * kRingSpacingUnits;
        for (int i = 0; i < kAttemptsPerRing; ++i) {
            const float angle =
                (kTwoPi * static_cast<float>(i)) / static_cast<float>(kAttemptsPerRing);
            const Vec2 candidate = anchorPos + FromAngle(angle) * ringRadius;
            if (ring == 1 && i == 0) {
                fallback = candidate;  // Closest ring's first attempt, if nothing ever clears.
            }
            if (IsClear(registry, self, candidate, margin)) {
                return candidate;
            }
        }
    }
    return fallback;
}

// architecture.md 12.36: how far outside a DockingBay's own position a rig appears, so it reads
// as having just launched from the bay rather than materializing on top of the station's hull.
constexpr float kBayExitOffsetUnits = 80.0f;

// Nearest friendly (same-faction) DockingBay to `referencePosition`, and the point just outside
// it a rig should appear at -- offset along the vector from the bay's own station root to the
// bay itself, so the exit point reads as "away from the hull." entt::null bay if no friendly
// DockingBay exists anywhere in the registry.
struct BayExit {
    entt::entity bay = entt::null;
    Vec2 position{};
};

BayExit FindNearestFriendlyBayExit(const entt::registry& registry, const FactionId& faction,
                                   const Vec2& referencePosition) {
    BayExit best;
    float bestDistSq = std::numeric_limits<float>::max();

    for (auto [bay, parent, bayXf] :
         registry.view<DockingBay, ParentRig, WorldTransform>().each()) {
        const auto* stationFaction = registry.try_get<FactionRef>(parent.root);
        if (stationFaction == nullptr || !(stationFaction->id == faction)) {
            continue;
        }
        const float distSq = DistanceSquared(referencePosition, bayXf.position);
        if (distSq >= bestDistSq) {
            continue;
        }
        bestDistSq = distSq;
        best.bay = bay;

        // Radially outward from the station root through the bay. Falls back to +x for the
        // degenerate case of a bay mounted at its root's own position (Normalized already
        // returns the zero vector there rather than NaN; this picks a direction from it).
        Vec2 direction{1.0f, 0.0f};
        if (const auto* stationXf = registry.try_get<WorldTransform>(parent.root)) {
            const Vec2 outward = Normalized(bayXf.position - stationXf->position);
            if (!(outward == Vec2{0.0f, 0.0f})) {
                direction = outward;
            }
        }
        best.position = bayXf.position + direction * kBayExitOffsetUnits;
    }
    return best;
}

void ResolveRespawns(entt::registry& registry) {
    std::vector<entt::entity> resolved;
    for (auto [self, respawn, xf, faction] :
         registry.view<RespawnPending, WorldTransform, FactionRef>().each()) {
        const std::optional<Vec2> placement =
            ResolveSpawnPlacement(registry, faction.id, xf.position, self, respawn.marginUnits);
        if (!placement) {
            continue;  // No friendly bay or anchor yet -- retry once one exists.
        }
        xf.position = *placement;
        resolved.push_back(self);
    }
    for (const entt::entity self : resolved) {
        registry.remove<RespawnPending>(self);
    }
}

void CullFarRigs(entt::registry& registry) {
    std::vector<entt::entity> toCull;
    for (auto [self, xf, rig] :
         registry.view<WorldTransform, Rig>(entt::exclude<PlayerControlled>).each()) {
        if (registry.all_of<SpawnAnchor>(self)) {
            continue;  // An anchor cannot cull itself out from under everything measured on it.
        }
        entt::entity anchor = entt::null;
        float distance = 0.0f;
        if (FindNearestAnchor(registry, xf.position, anchor, distance) &&
            distance > kCullRadiusUnits) {
            toCull.push_back(self);
            for (const entt::entity hardpoint : rig.children) {
                if (registry.valid(hardpoint)) {
                    toCull.push_back(hardpoint);
                }
            }
        }
    }
    for (const entt::entity entity : toCull) {
        registry.destroy(entity);
    }
}

}  // namespace

std::optional<Vec2> ResolveSpawnPlacement(const entt::registry& registry, const FactionId& faction,
                                          const Vec2& referencePosition, entt::entity self,
                                          float marginUnits) {
    if (const BayExit bay = FindNearestFriendlyBayExit(registry, faction, referencePosition);
        bay.bay != entt::null) {
        if (IsClear(registry, self, bay.position, marginUnits)) {
            return bay.position;
        }
        return FindSafePlacement(registry, self, bay.position, marginUnits);
    }

    entt::entity anchor = entt::null;
    float unusedDistance = 0.0f;
    if (FindNearestAnchor(registry, referencePosition, anchor, unusedDistance)) {
        return FindSafePlacement(registry, self, registry.get<WorldTransform>(anchor).position,
                                 marginUnits);
    }

    return std::nullopt;  // No friendly bay or anchor anywhere -- nothing to place against yet.
}

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    ResolveRespawns(registry);
    CullFarRigs(registry);
}

}  // namespace sr::space::spawn_system
