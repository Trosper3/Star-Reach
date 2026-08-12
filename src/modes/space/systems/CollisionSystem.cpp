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
#include "shared/math/Angle.h"
#include "shared/math/Vec2.h"

namespace sr::space::collision_system {
namespace {

constexpr float kCellSize = 300.0f;
constexpr int kHullSamplesPerHardpoint = 8;
constexpr float kRestitution = 0.65f;
constexpr float kRamDamagePerSpeed = 0.15f;
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

// Andrew's monotone chain, O(n log n). Winding is whatever falls out of the sort -- PolygonOverlap
// below is winding-agnostic. Returns empty if fewer than 3 distinct points survive dedup.
std::vector<Vec2> ConvexHull(std::vector<Vec2> pts) {
    std::sort(pts.begin(), pts.end(),
              [](const Vec2& a, const Vec2& b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
    pts.erase(
        std::unique(pts.begin(), pts.end(), [](const Vec2& a, const Vec2& b) { return a == b; }),
        pts.end());
    if (pts.size() < 3) {
        return {};
    }

    const auto cross = [](const Vec2& o, const Vec2& a, const Vec2& b) {
        return Cross(a - o, b - o);
    };

    const size_t n = pts.size();
    std::vector<Vec2> hull(2 * n);
    int k = 0;
    for (size_t i = 0; i < n; ++i) {
        while (k >= 2 && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) {
            --k;
        }
        hull[k++] = pts[i];
    }
    const int lower = k + 1;
    for (int i = static_cast<int>(n) - 2; i >= 0; --i) {
        while (k >= lower && cross(hull[k - 2], hull[k - 1], pts[i]) <= 0.0f) {
            --k;
        }
        hull[k++] = pts[i];
    }
    hull.resize(k - 1);
    return hull;
}

// Samples points around every living hardpoint's HitRadius circle and hulls them. There is no
// sprite art to trace yet (architecture.md section 6, deferred) -- the rig's actual mount layout
// stands in for it, which has the advantage of never drifting out of sync with the hardpoints
// DamageSystem is actually destroying.
std::vector<Vec2> BuildWorldHull(const entt::registry& registry, const Rig& rig) {
    std::vector<Vec2> points;
    points.reserve(rig.children.size() * kHullSamplesPerHardpoint);
    for (const entt::entity child : rig.children) {
        if (registry.all_of<Destroyed>(child)) {
            continue;
        }
        const auto* xf = registry.try_get<WorldTransform>(child);
        const auto* hit = registry.try_get<HitRadius>(child);
        if (xf == nullptr || hit == nullptr) {
            continue;
        }
        for (int i = 0; i < kHullSamplesPerHardpoint; ++i) {
            const float angle = kTwoPi * static_cast<float>(i) / kHullSamplesPerHardpoint;
            points.push_back(xf->position + Vec2{std::cos(angle), std::sin(angle)} * hit->value);
        }
    }
    return ConvexHull(std::move(points));
}

Vec2 Centroid(const std::vector<Vec2>& pts) {
    Vec2 c{};
    for (const Vec2& p : pts) {
        c += p;
    }
    return c * (1.0f / static_cast<float>(pts.size()));
}

void ProjectOntoAxis(const std::vector<Vec2>& poly, const Vec2& axis, float& outMin,
                     float& outMax) {
    outMin = outMax = Dot(poly[0], axis);
    for (size_t i = 1; i < poly.size(); ++i) {
        const float p = Dot(poly[i], axis);
        outMin = std::min(outMin, p);
        outMax = std::max(outMax, p);
    }
}

// Tests every edge normal of `poly` as a candidate separating axis, tracking the axis with the
// smallest overlap (the eventual MTV candidate) as it goes. Returns false the instant a true
// separating axis is found.
bool TestEdgeAxes(const std::vector<Vec2>& poly, const std::vector<Vec2>& other, float& bestDepth,
                  Vec2& bestAxis) {
    const size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        const Vec2 edge = poly[(i + 1) % n] - poly[i];
        const Vec2 axis = Normalized(Vec2{-edge.y, edge.x});
        float aMin = 0.0f, aMax = 0.0f, bMin = 0.0f, bMax = 0.0f;
        ProjectOntoAxis(poly, axis, aMin, aMax);
        ProjectOntoAxis(other, axis, bMin, bMax);
        if (aMax < bMin || bMax < aMin) {
            return false;
        }
        const float depth = std::min(aMax, bMax) - std::max(aMin, bMin);
        if (depth < bestDepth) {
            bestDepth = depth;
            bestAxis = axis;
        }
    }
    return true;
}

// Separating Axis Theorem overlap test, ported from StarReach2's CollisionHull.cpp. On overlap,
// outMTV points from b's centroid toward a's and outDepth is the minimum distance along it that
// separates the two hulls.
bool PolygonOverlap(const std::vector<Vec2>& a, const std::vector<Vec2>& b, Vec2& outMTV,
                    float& outDepth) {
    if (a.size() < 3 || b.size() < 3) {
        return false;
    }

    float bestDepth = FLT_MAX;
    Vec2 bestAxis{};
    if (!TestEdgeAxes(a, b, bestDepth, bestAxis) || !TestEdgeAxes(b, a, bestDepth, bestAxis)) {
        return false;
    }

    if (Dot(Centroid(a) - Centroid(b), bestAxis) < 0.0f) {
        bestAxis = bestAxis * -1.0f;
    }

    outMTV = bestAxis;
    outDepth = bestDepth;
    return true;
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

    const float baseDamage = kRamDamagePerSpeed * speedChange;
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
// from StarReach2's ResolveMassCollision, minus the pure-circle fallback -- every rig has a real
// hull here (BuildWorldHull), so there is no craft type left that needs one.
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
    const std::vector<Vec2> hullA = BuildWorldHull(registry, rigA);
    const std::vector<Vec2> hullB = BuildWorldHull(registry, rigB);

    Vec2 mtv{};
    float depth = 0.0f;
    if (!PolygonOverlap(hullA, hullB, mtv, depth)) {
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
