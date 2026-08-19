#include "modes/space/systems/CollisionSystem.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "shared/components/Docking.h"
#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"

namespace sr::space::collision_system {
namespace {

constexpr float kCellSize = 300.0f;
constexpr float kRestitution = 0.65f;
// Kinetic energy goes as half m v^2 (architecture.md 12.22): scaled by the pair's reduced mass so
// two dreadnoughts colliding no longer deal the same base damage as two fighters at the same
// speed change -- only the heavy/light split did before this. Placeholder magnitude, tuned
// against this file's own test fixtures rather than shipped content masses; T-05's combat balance
// pass owns the final number once real hull masses (architecture.md 12.19) are widely authored.
constexpr float kRamDamageScale = 0.00006f;
constexpr float kRamCooldownSeconds = 1.2f;

uint64_t PackCell(int32_t cx, int32_t cy) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(cx)) << 32) | static_cast<uint32_t>(cy);
}

// Broad-phase spatial hash, rebuilt fresh every tick from this tick's live rig roots -- ported
// from StarReach2's SpatialGrid.h. kCellSize must be >= the largest CollisionRadius any rig can
// have; Query() only visits the query point's own cell and its 8 neighbors.
class SpatialGrid {
public:
    void Insert(int index, const Vec2& pos) { cells_[Key(pos)].push_back(index); }

    // Appends every index sharing pos's cell or a neighbor onto `out`. Not deduplicated across
    // cells -- callers filter by index order (see Tick) to avoid resolving a pair twice.
    void Query(const Vec2& pos, std::vector<int>& out) const {
        const int32_t cx = static_cast<int32_t>(std::floor(pos.x / kCellSize));
        const int32_t cy = static_cast<int32_t>(std::floor(pos.y / kCellSize));
        for (int32_t dy = -1; dy <= 1; ++dy) {
            for (int32_t dx = -1; dx <= 1; ++dx) {
                const auto it = cells_.find(PackCell(cx + dx, cy + dy));
                if (it != cells_.end()) {
                    out.insert(out.end(), it->second.begin(), it->second.end());
                }
            }
        }
    }

private:
    uint64_t Key(const Vec2& pos) const {
        return PackCell(static_cast<int32_t>(std::floor(pos.x / kCellSize)),
                        static_cast<int32_t>(std::floor(pos.y / kCellSize)));
    }

    std::unordered_map<uint64_t, std::vector<int>> cells_;
};

// One living hardpoint's world-space collision circle.
struct HardpointCircle {
    Vec2 position;
    float radius;
};

// Every living hardpoint's HitRadius circle, in world space -- the narrow-phase collidable shape
// itself (architecture.md 12.22), not a hull sampled from it. There is no sprite art to trace yet
// (architecture.md section 6, deferred), and unlike a hull rebuilt each tick from the same living
// set, per-hardpoint circles let a destroyed hardpoint open an exact hole a shot -- or another
// hull -- can cross, matching P0-01/13.3 finding AB's "draw exactly what you test."
std::vector<HardpointCircle> LivingHardpointCircles(const entt::registry& registry,
                                                    const Rig& rig) {
    std::vector<HardpointCircle> circles;
    circles.reserve(rig.children.size());
    for (const entt::entity child : rig.children) {
        if (registry.all_of<Destroyed>(child)) {
            continue;
        }
        const auto* xf = registry.try_get<WorldTransform>(child);
        const auto* hit = registry.try_get<HitRadius>(child);
        if (xf == nullptr || hit == nullptr) {
            continue;
        }
        circles.push_back({xf->position, hit->value});
    }
    return circles;
}

// Deepest-penetration pair among every living hardpoint on each side, replacing the old
// convex-hull SAT (architecture.md 12.22). Bounded cost: (living hardpoints in a) x (living
// hardpoints in b) circle tests per candidate pair, after the broad phase already narrowed
// candidates by CollisionRadius. On overlap, outMTV points from b's side toward a's and outDepth
// is how far apart that pair still needs to move to stop touching -- the same convention
// ResolvePair's push-apart and ApplyRamDamage already expect from the old polygon SAT.
bool NarrowPhaseOverlap(const std::vector<HardpointCircle>& a,
                        const std::vector<HardpointCircle>& b, Vec2& outMTV, float& outDepth) {
    bool found = false;
    float bestDepth = 0.0f;
    Vec2 bestMTV{};
    for (const HardpointCircle& ca : a) {
        for (const HardpointCircle& cb : b) {
            const Vec2 delta = ca.position - cb.position;
            const float radiusSum = ca.radius + cb.radius;
            const float distSq = LengthSquared(delta);
            if (distSq >= radiusSum * radiusSum) {
                continue;
            }
            const float dist = std::sqrt(distSq);
            const float depth = radiusSum - dist;
            if (!found || depth > bestDepth) {
                found = true;
                bestDepth = depth;
                bestMTV = dist > 0.0001f ? delta * (1.0f / dist) : Vec2{1.0f, 0.0f};
            }
        }
    }
    if (found) {
        outMTV = bestMTV;
        outDepth = bestDepth;
    }
    return found;
}

// Nearest living, damageable hardpoint to `point` -- the ram-damage aim point, same idea as
// ProjectileSystem's hit selection but by proximity rather than a swept segment.
entt::entity NearestLivingHardpoint(const entt::registry& registry, const Rig& rig,
                                    const Vec2& point) {
    entt::entity best = entt::null;
    float bestDistSq = FLT_MAX;
    for (const entt::entity child : rig.children) {
        if (!registry.all_of<Health>(child) || registry.all_of<Destroyed>(child)) {
            continue;
        }
        const auto* xf = registry.try_get<WorldTransform>(child);
        if (xf == nullptr) {
            continue;
        }
        const float distSq = DistanceSquared(xf->position, point);
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            best = child;
        }
    }
    return best;
}

// Accumulates rather than overwrites, matching ProjectileSystem's QueueDamage -- two collisions
// landing on the same hardpoint in one tick must not lose one to the other.
void QueueDamage(entt::registry& registry, entt::entity hardpoint, float amount,
                 entt::entity source) {
    if (hardpoint == entt::null || amount <= 0.0f) {
        return;
    }
    if (auto* pending = registry.try_get<PendingDamage>(hardpoint)) {
        pending->amount += amount;
        pending->type = DamageType::Kinetic;
        pending->source = source;
    } else {
        registry.emplace<PendingDamage>(hardpoint, amount, DamageType::Kinetic, source);
    }
}

// Splits a ramming impact into damage on the hardpoint nearest the point of contact on each side,
// same mass-ratio split as the velocity impulse: the lighter rig takes more, the heavier less.
// Gated by RamCooldown so two hulls still overlapping next tick don't exchange damage every tick
// before they separate.
void ApplyRamDamage(entt::registry& registry, entt::entity rootA, entt::entity rootB,
                    const Rig& rigA, const Rig& rigB, const Vec2& posA, const Vec2& posB,
                    float massA, float massB, float speedChange) {
    auto& cooldownA = registry.get<RamCooldown>(rootA);
    auto& cooldownB = registry.get<RamCooldown>(rootB);
    if (speedChange <= 0.0f || cooldownA.secondsRemaining > 0.0f ||
        cooldownB.secondsRemaining > 0.0f) {
        return;
    }

    // Reduced mass, per architecture.md 12.22's ½mv² note -- this is what makes total ram damage
    // grow with the pair's absolute mass, not just its split between the two sides.
    const float reducedMass = (massA * massB) / (massA + massB);
    const float baseDamage = kRamDamageScale * 0.5f * reducedMass * speedChange * speedChange;
    const bool aHeavier = massA >= massB;
    const float heavyShare = (aHeavier ? massB : massA) / (massA + massB);
    const float lightShare = 1.0f - heavyShare;

    QueueDamage(registry, NearestLivingHardpoint(registry, rigA, posB),
                baseDamage * (aHeavier ? heavyShare : lightShare), rootB);
    QueueDamage(registry, NearestLivingHardpoint(registry, rigB, posA),
                baseDamage * (aHeavier ? lightShare : heavyShare), rootA);

    cooldownA.secondsRemaining = kRamCooldownSeconds;
    cooldownB.secondsRemaining = kRamCooldownSeconds;
}

// Asymmetric elastic collision: the heavier side does not move at all; the lighter side absorbs
// the full position correction and a velocity change scaled by how much lighter it is. Ported
// from StarReach2's ResolveMassCollision, minus the pure-circle fallback -- every rig resolves
// through its own living hardpoint circles here (LivingHardpointCircles), so there is no craft
// type left that needs one.
void ResolvePair(entt::registry& registry, entt::entity rootA, entt::entity rootB) {
    auto& xfA = registry.get<WorldTransform>(rootA);
    auto& xfB = registry.get<WorldTransform>(rootB);
    const float broadRadius =
        registry.get<CollisionRadius>(rootA).value + registry.get<CollisionRadius>(rootB).value;
    if (DistanceSquared(xfA.position, xfB.position) >= broadRadius * broadRadius) {
        return;
    }

    const Rig& rigA = registry.get<Rig>(rootA);
    const Rig& rigB = registry.get<Rig>(rootB);
    const std::vector<HardpointCircle> circlesA = LivingHardpointCircles(registry, rigA);
    const std::vector<HardpointCircle> circlesB = LivingHardpointCircles(registry, rigB);

    Vec2 mtv{};
    float depth = 0.0f;
    if (!NarrowPhaseOverlap(circlesA, circlesB, mtv, depth)) {
        return;
    }

    auto& velA = registry.get<Velocity>(rootA);
    auto& velB = registry.get<Velocity>(rootB);
    const float massA = registry.get<BodyMass>(rootA).kilograms;
    const float massB = registry.get<BodyMass>(rootB).kilograms;
    const bool aHeavier = massA >= massB;
    const float heavy = aHeavier ? massA : massB;
    const float light = aHeavier ? massB : massA;
    const float lightShare = heavy / (heavy + light);

    if (aHeavier) {
        xfB.position -= mtv * depth;
    } else {
        xfA.position += mtv * depth;
    }

    const float relativeSpeed = Dot(velA.linear - velB.linear, mtv);
    if (relativeSpeed >= 0.0f) {
        return;  // Already separating along the MTV.
    }

    const float speedChange = -(1.0f + kRestitution) * relativeSpeed * lightShare;
    if (aHeavier) {
        velB.linear -= mtv * speedChange;
    } else {
        velA.linear += mtv * speedChange;
    }

    ApplyRamDamage(registry, rootA, rootB, rigA, rigB, xfA.position, xfB.position, massA, massB,
                   speedChange);
}

void TickRamCooldowns(entt::registry& registry, float dt) {
    for (auto [entity, cooldown] : registry.view<RamCooldown>().each()) {
        (void)entity;
        cooldown.secondsRemaining = std::max(0.0f, cooldown.secondsRemaining - dt);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    TickRamCooldowns(registry, ctx.dt);

    std::vector<entt::entity> roots;
    SpatialGrid grid;
    // exclude<Docked>: features.md 3.4's "a docked vessel is not a target" -- the exclusion half
    // architecture.md 12.34 specifies (the other half is DamageSystem's cascade destruction).
    const auto candidates =
        registry.view<Rig, WorldTransform, CollisionRadius, BodyMass, Velocity, RamCooldown>(
            entt::exclude<Destroyed, Docked>);
    for (const entt::entity root : candidates) {
        grid.Insert(static_cast<int>(roots.size()), registry.get<WorldTransform>(root).position);
        roots.push_back(root);
    }

    std::vector<int> nearby;
    for (size_t i = 0; i < roots.size(); ++i) {
        nearby.clear();
        grid.Query(registry.get<WorldTransform>(roots[i]).position, nearby);
        for (const int j : nearby) {
            if (j <= static_cast<int>(i)) {
                continue;  // Already resolved from j's own query, or self.
            }
            ResolvePair(registry, roots[i], roots[static_cast<size_t>(j)]);
        }
    }
}

}  // namespace sr::space::collision_system
