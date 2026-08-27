#include "modes/space/ui/StatusProjection.h"

#include <raylib.h>
#include <algorithm>
#include <cmath>

#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/rig/ModuleAttachment.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::status_projection {
namespace {

// Fixed screen-pixel size for one node's circle -- features.md 3.9's "element size is fixed at
// whatever is legible; detail adapts," never the reverse. Layout position scales with the
// projection's diameter; this never does.
constexpr float kNodeRadius = 5.0f;
constexpr float kNodeGap = 3.0f;
constexpr float kMinNodeSpacing = kNodeRadius * 2.0f + kNodeGap;
// Leaves room so an edge node's own circle never touches the projection's outer boundary.
constexpr float kLayoutMargin = 0.85f;
constexpr float kLoopThickness = 2.0f;
constexpr float kLoopMargin = 4.0f;  // Clearance between a node's circle and the loop enclosing it.
constexpr int kLoopSegments = 32;
constexpr int kMarkerRingSegments = 48;
constexpr int kGlyphFontSize = 10;

Vector2 ToRaylib(const Vec2& v) {
    return Vector2{v.x, v.y};
}

Vec2 LocalOffsetOf(const entt::registry& registry, entt::entity entity) {
    const auto* local = registry.try_get<LocalTransform>(entity);
    return local != nullptr ? local->offset : Vec2{0.0f, 0.0f};
}

float HullRadiusFor(const entt::registry& registry, entt::entity rigRoot) {
    const auto* collision = registry.try_get<CollisionRadius>(rigRoot);
    return collision != nullptr && collision->value > 0.0f ? collision->value : 1.0f;
}

float LayoutScale(float hullRadius, float diameterPixels) {
    return (diameterPixels * 0.5f * kLayoutMargin) / std::max(hullRadius, 1.0f);
}

bool WouldOverlap(const std::vector<Vec2>& offsets, float scale) {
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        for (std::size_t j = i + 1; j < offsets.size(); ++j) {
            if (Distance(offsets[i], offsets[j]) * scale < kMinNodeSpacing) {
                return true;
            }
        }
    }
    return false;
}

float HardpointIntegrity(const entt::registry& registry, entt::entity hardpoint) {
    // The one rule this file exists to never violate: a Destroyed hardpoint reads as 0 regardless
    // of whatever Health::current it still carries -- DamageSystem's structural cascade tags
    // Destroyed without zeroing Health (shared/rig/ModuleAttachment.h's own comment on why).
    if (registry.all_of<Destroyed>(hardpoint)) {
        return 0.0f;
    }
    const auto* health = registry.try_get<Health>(hardpoint);
    return (health != nullptr && health->max > 0.0f)
               ? std::clamp(health->current / health->max, 0.0f, 1.0f)
               : 0.0f;
}

// Walks StructuralAttachment upward from `hardpoint` to the nearest Chassis-or-Armor boundary --
// the "chassis -> armour segments -> functional mounts" tree features.md 3.9 collapses onto at
// DetailLevel::Segments. Always terminates: RigFactory writes a StructuralAttachment on every
// hardpoint including the chassis itself (attachedTo == entt::null there), and the chain a valid
// rig authors is acyclic.
entt::entity SegmentKeyFor(const entt::registry& registry, entt::entity hardpoint) {
    entt::entity current = hardpoint;
    while (true) {
        const auto* role = registry.try_get<ShellRole>(current);
        if (role != nullptr &&
            (role->kind == ShellKind::Chassis || role->kind == ShellKind::Armor)) {
            return current;
        }
        const auto* attachment = registry.try_get<StructuralAttachment>(current);
        if (attachment == nullptr || attachment->attachedTo == entt::null) {
            return current;
        }
        current = attachment->attachedTo;
    }
}

std::vector<Vec2> SegmentOffsets(const entt::registry& registry,
                                 const std::vector<entt::entity>& hardpoints) {
    std::vector<entt::entity> keys;
    for (const entt::entity hardpoint : hardpoints) {
        const entt::entity key = SegmentKeyFor(registry, hardpoint);
        if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
            keys.push_back(key);
        }
    }
    std::vector<Vec2> offsets;
    offsets.reserve(keys.size());
    for (const entt::entity key : keys) {
        offsets.push_back(LocalOffsetOf(registry, key));
    }
    return offsets;
}

HardpointNode BuildHardpointNode(const entt::registry& registry, entt::entity hardpoint) {
    HardpointNode node;
    node.localOffset = LocalOffsetOf(registry, hardpoint);
    node.integrity = HardpointIntegrity(registry, hardpoint);
    if (const auto* role = registry.try_get<ShellRole>(hardpoint)) {
        node.kind = role->kind;
    }
    return node;
}

std::vector<SegmentNode> BuildSegments(const entt::registry& registry, const Rig& rig) {
    std::vector<entt::entity> keys;
    std::vector<std::pair<float, float>> currentAndMax;  // index-aligned with keys
    for (const entt::entity hardpoint : rig.children) {
        const entt::entity key = SegmentKeyFor(registry, hardpoint);
        auto found = std::find(keys.begin(), keys.end(), key);
        if (found == keys.end()) {
            keys.push_back(key);
            currentAndMax.emplace_back(0.0f, 0.0f);
            found = keys.end() - 1;
        }
        auto& totals = currentAndMax[static_cast<std::size_t>(found - keys.begin())];
        if (const auto* health = registry.try_get<Health>(hardpoint)) {
            totals.second += health->max;
            if (!registry.all_of<Destroyed>(hardpoint)) {
                totals.first += health->current;
            }
        }
    }

    std::vector<SegmentNode> segments;
    segments.reserve(keys.size());
    for (std::size_t i = 0; i < keys.size(); ++i) {
        SegmentNode node;
        node.localOffset = LocalOffsetOf(registry, keys[i]);
        const auto& [totalCurrent, totalMax] = currentAndMax[i];
        node.integrity = totalMax > 0.0f ? std::clamp(totalCurrent / totalMax, 0.0f, 1.0f) : 0.0f;
        segments.push_back(node);
    }
    return segments;
}

// Personal/Bubble: the centroid of every hardpoint the shield covers, and the farthest covered
// member's distance from it -- Draw() adds the node radius and a fixed margin on top, in screen
// pixels, so the loop always clears the circles it encloses regardless of scale. Conformal skips
// this entirely: "around the hull outline" is the rig's own extent centred on the root, not a
// shape built from member positions (features.md 3.9's own table).
ShieldLoop BuildLoopGeometry(const Shield& shield, const std::vector<Vec2>& covered,
                             float hullRadius) {
    ShieldLoop loop;
    loop.absorbs = shield.absorbs;
    loop.chargeFraction =
        shield.max > 0.0f ? std::clamp(shield.current / shield.max, 0.0f, 1.0f) : 0.0f;

    if (shield.coverage == ShieldCoverage::Conformal) {
        loop.localCenter = Vec2{0.0f, 0.0f};
        loop.localRadius = hullRadius;
        return loop;
    }

    Vec2 centroid{0.0f, 0.0f};
    for (const Vec2& offset : covered) {
        centroid += offset;
    }
    if (!covered.empty()) {
        centroid = centroid / static_cast<float>(covered.size());
    }
    float maxDistance = 0.0f;
    for (const Vec2& offset : covered) {
        maxDistance = std::max(maxDistance, Distance(centroid, offset));
    }
    loop.localCenter = centroid;
    loop.localRadius = maxDistance;
    return loop;
}

std::vector<ShieldLoop> BuildShieldLoops(const entt::registry& registry, const Rig& rig,
                                         float hullRadius) {
    std::vector<ShieldLoop> loops;
    for (const entt::entity shieldEntity : rig.children) {
        if (registry.all_of<Destroyed>(shieldEntity)) {
            continue;
        }
        const auto* shield = registry.try_get<Shield>(shieldEntity);
        if (shield == nullptr) {
            continue;
        }
        std::vector<Vec2> covered;
        for (const entt::entity candidate : rig.children) {
            if (!registry.all_of<Destroyed>(candidate) &&
                rig_attachment::ShieldCovers(registry, shieldEntity, candidate)) {
                covered.push_back(LocalOffsetOf(registry, candidate));
            }
        }
        loops.push_back(BuildLoopGeometry(*shield, covered, hullRadius));
    }
    return loops;
}

// One-letter monogram per ShellKind (features.md 3.9's glyph-carries-identity rule) -- the same
// C/A/P/E/W/S/F table ModulesMenu.cpp/RepairScreenModel.cpp/EngineeringScreenModel.cpp each
// already carry their own copy of, per this codebase's established "neither file may depend on
// the other" convention for small per-screen mapping tables.
char ShellGlyphChar(ShellKind kind) {
    switch (kind) {
        case ShellKind::Chassis: return 'C';
        case ShellKind::Armor: return 'A';
        case ShellKind::PowerCell: return 'P';
        case ShellKind::Engine: return 'E';
        case ShellKind::Weapon: return 'W';
        case ShellKind::Shield: return 'S';
        case ShellKind::Facility: return 'F';
    }
    return '?';
}

Vector2 ToScreen(const Vec2& localOffset, Vec2 screenCenter, float scale) {
    // Nose-up remap: root +x reads as screen "up," +y as screen "right" -- the same convention
    // EngineeringScreen's own schematic uses, so the two widgets read consistently.
    return ToRaylib(
        Vec2{screenCenter.x + localOffset.y * scale, screenCenter.y - localOffset.x * scale});
}

void DrawGlyph(Vector2 center, ShellKind kind) {
    const char glyph[2] = {ShellGlyphChar(kind), '\0'};
    const int width = MeasureText(glyph, kGlyphFontSize);
    DrawText(glyph, static_cast<int>(center.x) - width / 2,
             static_cast<int>(center.y) - kGlyphFontSize / 2, kGlyphFontSize, BLACK);
}

void DrawLoop(const ShieldLoop& loop, Vec2 screenCenter, float scale) {
    if (loop.chargeFraction <= 0.0f) {
        return;  // Shield down: no loop at all, never a phantom coverage claim.
    }
    const Vector2 center = ToScreen(loop.localCenter, screenCenter, scale);
    const float radius = loop.localRadius * scale + kNodeRadius + kLoopMargin;
    const Color color = sr::ui::DamageTypeColor(loop.absorbs);

    // Dash density as one continuum, never arc length (features.md 3.9's own rejected-alternative
    // note): evenly distributing `onSegments` of `kLoopSegments` slices gives a solid ring at full
    // charge, alternating dashes at mid charge, and sparse dots near zero -- one formula, no level
    // branch, and the full circumference is always represented at every charge.
    const int onSegments =
        std::max(1, static_cast<int>(std::lround(kLoopSegments * loop.chargeFraction)));
    const float degreesPerSegment = 360.0f / static_cast<float>(kLoopSegments);
    for (int i = 0; i < kLoopSegments; ++i) {
        if ((i * onSegments) % kLoopSegments >= onSegments) {
            continue;
        }
        const float start = static_cast<float>(i) * degreesPerSegment;
        DrawRing(center, radius - kLoopThickness * 0.5f, radius + kLoopThickness * 0.5f, start,
                 start + degreesPerSegment, 2, color);
    }
}

}  // namespace

DetailLevel ChooseDetailLevel(const entt::registry& registry, entt::entity rigRoot,
                              float diameterPixels) {
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr || rig->children.empty()) {
        return DetailLevel::Marker;
    }
    const float scale = LayoutScale(HullRadiusFor(registry, rigRoot), diameterPixels);

    std::vector<Vec2> hardpointOffsets;
    hardpointOffsets.reserve(rig->children.size());
    for (const entt::entity hardpoint : rig->children) {
        hardpointOffsets.push_back(LocalOffsetOf(registry, hardpoint));
    }
    if (!WouldOverlap(hardpointOffsets, scale)) {
        return DetailLevel::Full;
    }
    if (!WouldOverlap(SegmentOffsets(registry, rig->children), scale)) {
        return DetailLevel::Segments;
    }
    return DetailLevel::Marker;
}

Projection Build(const entt::registry& registry, entt::entity rigRoot, float diameterPixels) {
    Projection projection;
    projection.level = ChooseDetailLevel(registry, rigRoot, diameterPixels);
    projection.wholeRigIntegrity = rig_attachment::AggregateStructuralIntegrity(registry, rigRoot);
    projection.hullRadius = HullRadiusFor(registry, rigRoot);

    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr || projection.level == DetailLevel::Marker) {
        return projection;
    }

    if (projection.level == DetailLevel::Full) {
        for (const entt::entity hardpoint : rig->children) {
            projection.hardpoints.push_back(BuildHardpointNode(registry, hardpoint));
        }
        projection.loops = BuildShieldLoops(registry, *rig, projection.hullRadius);
    } else {
        projection.segments = BuildSegments(registry, *rig);
    }
    return projection;
}

void Draw(const Projection& projection, Vec2 screenCenter, float diameterPixels) {
    const float scale = LayoutScale(projection.hullRadius, diameterPixels);

    if (projection.level == DetailLevel::Marker) {
        const Color outline = sr::ui::IntegrityColor(projection.wholeRigIntegrity);
        DrawRing(ToRaylib(screenCenter), diameterPixels * 0.5f - kLoopThickness,
                 diameterPixels * 0.5f, 0.0f, 360.0f, kMarkerRingSegments, outline);
        return;
    }

    if (projection.level == DetailLevel::Segments) {
        for (const SegmentNode& segment : projection.segments) {
            DrawCircleV(ToScreen(segment.localOffset, screenCenter, scale), kNodeRadius,
                        sr::ui::IntegrityColor(segment.integrity));
        }
        return;
    }

    for (const ShieldLoop& loop : projection.loops) {
        DrawLoop(loop, screenCenter, scale);
    }
    for (const HardpointNode& node : projection.hardpoints) {
        const Vector2 center = ToScreen(node.localOffset, screenCenter, scale);
        DrawCircleV(center, kNodeRadius, sr::ui::IntegrityColor(node.integrity));
        DrawGlyph(center, node.kind);
    }
}

}  // namespace sr::space::ui::status_projection
