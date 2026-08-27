#pragma once

#include <cstdint>
#include <entt/entity/registry.hpp>
#include <vector>

#include "shared/blueprints/Taxonomy.h"
#include "shared/math/Vec2.h"

// modes/space/ui/ -- the status projection (features.md section 3.9, architecture.md 13.5 group
// 2e): "a schematic of a rig -- the hull outline, a circle per hardpoint, and a loop per shield.
// Colour carries condition, shape carries identity, and the loop's shape carries coverage."
//
// One object serves the player's own ship (wired into CockpitHud.cpp by this issue), the current
// target (features.md 3.10, a separate issue), and a degraded map marker (features.md 8.1,
// likewise separate) -- not three designs. This file owns the one shared Build()/Draw() pair; a
// future caller threads in a different rigRoot and diameter, nothing more.
//
// Pure data (Projection and everything that builds it) is raylib-free and unit-tested; Draw()
// below is the untested raylib half, the same split CockpitHud.cpp's own
// AggregateHullFraction/Draw already uses -- raylib's render batch is not valid without a live
// window (shared/ui/HudTheme.h's own comment on why its Draw* functions have no coverage either).
namespace sr::space::ui::status_projection {

// features.md 3.9's fit-based LOD: element size is fixed at whatever is legible, and detail
// collapses one level at a time when the next one down would overlap. `StructuralAttachment` is
// the collapse hierarchy (chassis -> armour segments -> functional mounts), so no layout is ever
// spatially untruthful -- "shoot the port flank" survives every collapse.
enum class DetailLevel : std::uint8_t {
    Full,      // Roomy: every living hardpoint, its own circle, coloured by its own integrity.
    Segments,  // Tight: chassis + armour segments, each coloured by its own subtree's aggregate.
    Marker,    // Hull outline only, coloured by whole-rig integrity.
};

// One drawable hardpoint at DetailLevel::Full. `localOffset` is root-relative, in world units
// (RigFactory's own LocalTransform, the live-entity twin of MountBlueprint::localOffset) --
// Draw() remaps it the same nose-up way EngineeringScreen's own schematic does, so the two read
// consistently. `integrity` is forced to 0 for a Destroyed hardpoint regardless of whatever its
// Health::current still holds (DamageSystem's cascade tags Destroyed without zeroing Health) --
// the one rule this file exists to never violate: a destroyed hardpoint never renders as healthy.
struct HardpointNode {
    Vec2 localOffset;
    float integrity = 0.0f;
    ShellKind kind = ShellKind::Armor;
};

// One collapsed node at DetailLevel::Segments: a chassis-or-armour boundary hardpoint plus every
// functional mount structurally attached beneath it, drawn as one dot at the boundary's own
// position, coloured by the aggregate of the whole subtree's living/max health.
struct SegmentNode {
    Vec2 localOffset;
    float integrity = 0.0f;
};

// One shield's coverage loop (DetailLevel::Full only -- collapsing hardpoints into segments
// removes the individual circles a loop would otherwise enclose). `localCenter`/`localRadius` are
// root-relative world units, already resolved from the shield's coverage mode (Personal: its own
// housing; Bubble: the centroid/reach of every hardpoint it covers; Conformal: the whole hull) --
// Draw() never branches on `mode` itself, only on the geometry already computed here.
struct ShieldLoop {
    Vec2 localCenter;
    float localRadius = 0.0f;
    // 0 = shield down (no loop drawn), 1 = full charge (a solid ring). Dash density, never arc
    // length, carries this -- features.md 3.9's rejected-alternatives note: a gap in the arc reads
    // as "unshielded" where the loop's shape already carries coverage (Bubble/Conformal).
    float chargeFraction = 0.0f;
    DamageType absorbs = DamageType::Kinetic;
};

struct Projection {
    DetailLevel level = DetailLevel::Marker;
    // Sum of living/max Health across every hardpoint on the rig (shared/rig/ModuleAttachment.h's
    // AggregateStructuralIntegrity) -- always populated, since Marker level has nothing else to
    // colour by and Full/Segments still want it for a "whole hull" readout alongside the detail.
    float wholeRigIntegrity = 0.0f;
    // World-unit half-extent (the rig's CollisionRadius) the layout normalises every offset
    // against, and the Marker level's own outline radius before Draw()'s screen-space scale.
    float hullRadius = 1.0f;
    std::vector<HardpointNode> hardpoints;  // DetailLevel::Full only.
    std::vector<SegmentNode> segments;      // DetailLevel::Segments only.
    std::vector<ShieldLoop> loops;          // DetailLevel::Full only.
};

// The collapse decision alone, exposed separately from Build() so it is independently
// unit-testable against synthetic layouts (features.md 3.9's own "degrades legibly at small
// sizes" test requirement) without needing a full rig's worth of Health/Shield data. Marker for a
// missing or childless Rig -- there is nothing to lay hardpoints out at all.
DetailLevel ChooseDetailLevel(const entt::registry& registry, entt::entity rigRoot,
                              float diameterPixels);

// Builds the full projection for `rigRoot`, sized to fit `diameterPixels`. Pure -- no raylib.
Projection Build(const entt::registry& registry, entt::entity rigRoot, float diameterPixels);

// Draws `projection` centred at `screenCenter`, `diameterPixels` across -- colour-is-condition
// (shared/ui/HudTheme.h's IntegrityColor), glyph-is-identity (a one-letter ShellKind monogram,
// Row.h's own placeholder convention, drawn with raylib's default font the same way
// IconRenderer::DrawMapMarker labels a contact -- this widget has no Fonts/Orbitron dependency of
// its own), outline-encloses-coverage with dash-density charge. Screen space, not world space --
// must be called outside BeginMode2D/EndMode2D, same as CockpitHud::Draw/IconRenderer.
void Draw(const Projection& projection, Vec2 screenCenter, float diameterPixels);

}  // namespace sr::space::ui::status_projection
