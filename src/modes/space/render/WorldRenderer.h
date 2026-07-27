#pragma once

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

// Draws every rig root, hardpoint, and projectile currently in `world`. `alpha` is the fixed-
// timestep interpolation fraction (SpaceFlight::InterpolationAlpha) -- each entity is drawn at
// PreviousTransform blended toward WorldTransform, not at raw simulation state, so motion stays
// smooth on displays faster than the 60 Hz simulation tick.
//
// Must be called between BeginDrawing/EndDrawing (engine::Window::BeginFrame/EndFrame).
void DrawWorld(const SystemWorld& world, const CameraView& camera, float alpha);

}  // namespace sr::space::render
