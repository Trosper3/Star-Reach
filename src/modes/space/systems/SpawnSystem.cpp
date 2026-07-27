#include "modes/space/systems/SpawnSystem.h"

#include <limits>
#include <vector>

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

void ResolveRespawns(entt::registry& registry) {
    std::vector<entt::entity> resolved;
    for (auto [self, respawn, xf] : registry.view<RespawnPending, WorldTransform>().each()) {
        entt::entity anchor = entt::null;
        float unusedDistance = 0.0f;
        if (!FindNearestAnchor(registry, xf.position, anchor, unusedDistance)) {
            continue;  // No anchor to respawn around yet -- retry once one exists.
        }
        const Vec2 anchorPos = registry.get<WorldTransform>(anchor).position;
        xf.position = FindSafePlacement(registry, self, anchorPos, respawn.marginUnits);
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

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    ResolveRespawns(registry);
    CullFarRigs(registry);
}

}  // namespace sr::space::spawn_system
