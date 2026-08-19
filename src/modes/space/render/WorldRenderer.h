#pragma once

#include <entt/entity/registry.hpp>
#include <vector>

#include "modes/space/data/SystemWorld.h"
#include "shared/math/Vec2.h"

// modes/space/render/ -- the presentation layer for the space mode (architecture.md section 3,
// first vertical slice step 8).
//
// Law 7: this is draw-only. It reads WorldTransform/PreviousTransform and the role components
// hardpoints already carry; it never computes gameplay state. raylib is included directly in
// WorldRenderer.cpp -- section 2.3's layer table only forbids raylib.h from shared/ and core/,
// and Vec2.h's own comment names a mode's render/ directory as exactly where the conversion to
// raylib's Vector2 belongs.
namespace sr::space::render {

// Presentation-only camera state (Law 6/7). Deliberately not raylib's Camera2D: SpaceFlight owns
// this as plain data so the type stays visible in a header nothing else needs raylib to read.
struct CameraView {
    Vec2 target;
    float zoom = 1.0f;
};

// Draws every world body (star/planet/wreck/drop/asteroid/anomaly), rig root, hardpoint, and
// projectile currently in `world`, in that order -- the full back-to-front draw order, stated
// once, here:
//
//   DrawWorldBodies    // Star -> Planet -> Wreck -> Drop -> Asteroid -> Anomaly (BodyKind order)
//   DrawShips          // rig roots -- a heading marker only; the hull is DrawHardpoints below
//   DrawHardpoints     // rig children
//   DrawProjectiles
//
// `alpha` is the fixed-timestep interpolation fraction (SpaceFlight::InterpolationAlpha) -- each
// entity is drawn at PreviousTransform blended toward WorldTransform, not at raw simulation
// state, so motion stays smooth on displays faster than the 60 Hz simulation tick. A world body
// that never moves (a drop, a wreck) carries no PreviousTransform and is drawn unblended.
//
// Must be called between BeginDrawing/EndDrawing (engine::Window::BeginFrame/EndFrame).
void DrawWorld(const SystemWorld& world, const CameraView& camera, float alpha);

// True if `entity` should draw a heading marker (has thrust) -- a station or a de-engined hulk
// draws none, only the hardpoint circles DrawHardpoints already renders for it. Pure with respect
// to the registry, so it is unit-testable without a window.
bool HasVisiblePropulsion(const entt::registry& registry, entt::entity entity);

// Every living hardpoint, in DrawHardpoints' draw order: ascending DrawLayer (features.md section
// 3.5's five-layer stack), tie-broken by ascending LocalTransform y so the stacking never pops as
// a hull turns (architecture.md 12.16 item 19). Pure with respect to the registry -- unit-testable
// without a window, the same pattern HasVisiblePropulsion above uses.
std::vector<entt::entity> SortedHardpointsForDraw(const entt::registry& registry);

}  // namespace sr::space::render
