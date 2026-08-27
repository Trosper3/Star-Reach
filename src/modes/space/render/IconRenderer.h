#pragma once

#include <cstdint>
#include <entt/entity/registry.hpp>
#include <optional>
#include <string>

#include "modes/space/render/WorldRenderer.h"
#include "shared/components/Physics.h"
#include "shared/math/Vec2.h"
#include "shared/ui/HudTheme.h"

// modes/space/render/ -- IconRenderer, alongside WorldRenderer (architecture.md section 3).
//
// WorldRenderer draws world-space sprites: shapes that pan and scale with the camera, drawn
// inside BeginMode2D/EndMode2D. IconRenderer is the distinct thing the issue names it as -- a
// fixed-pixel-size marker projected FROM world space TO screen space, so it stays legible
// regardless of zoom. Called after WorldRenderer, outside BeginMode2D/EndMode2D.
//
// "Map markers" (galaxy map territory blobs, sensor-gated hostile contacts) are consumed by
// modes/space/ui/NavigationMap.cpp and SensorContacts.cpp.
namespace sr::space::render {

// The PlayerControlled entity's current AimPoint, or nullopt if there is no player or the player
// has not aimed yet this run (AimPoint is written every real frame by PlayerInputSystem, draining
// FlightControls' AimIntent, so this is only nullopt before the first frame). Pure with respect
// to the registry -- no raylib -- so it is unit-testable without a live GL context, unlike
// WorldToScreen/DrawAimReticle below.
//
// architecture.md 13.3 finding Q: this deliberately replaces the old Target-based reticle rather
// than leaving it to silently draw nothing once TargetingSystem stopped acquiring for the player
// (features.md 3.2 -- there is no target lock to point a reticle at).
std::optional<Vec2> AimPointWorldPosition(const entt::registry& registry);

// Projects a world-space position into screen-space pixels under `camera`, using the identical
// Camera2D construction WorldRenderer::DrawWorld uses internally -- an icon and the sprite it
// marks stay aligned under panning and zoom.
Vec2 WorldToScreen(const Vec2& worldPosition, const CameraView& camera);

// The exact inverse of WorldToScreen -- the world-space point under a screen-space (e.g. mouse)
// position, under the identical Camera2D construction. FlightControls uses this to turn the
// cursor into the AimIntent it pushes every frame.
Vec2 ScreenToWorld(const Vec2& screenPosition, const CameraView& camera);

// Draws a bracket reticle (sr_shared_ui's DrawBracketPanel, no fill) at the player's current
// AimPoint, sized in fixed screen pixels so it does not shrink or grow with zoom. No-op if
// AimPointWorldPosition returns nullopt. Must be called between BeginDrawing/EndDrawing, OUTSIDE
// BeginMode2D/EndMode2D -- it does its own world-to-screen projection.
void DrawAimReticle(const entt::registry& registry, const CameraView& camera);

// Which vector shape a marker bakes to (architecture.md 12.35, 15.1 finding 20) -- a map-specific
// dispatch, deliberately not `sr::BodyKind` (shared/components/Physics.h): that enum names world
// bodies (Star, Planet, Wreck...) a map marker never represents, since Territory blobs and fog
// ghosts are aggregates/unknowns with no single body behind them.
enum class MapMarkerKind : std::uint8_t {
    Territory,  // A known territory-aggregate blob (architecture.md 12.35's Level-1 sub-scales).
    Unknown,    // A present-but-unknown blob -- Territory knows a claim exists here, the viewing
                // faction hasn't discovered it (features.md 8.3: "absence must never look like
                // emptiness").
    Hostile,    // A sensor-gated hostile contact (NavigationMap::VisibleHostileRigs).
};

// A marker at a SCREEN-space position the caller has already projected (WorldToScreen above, or
// a map's own layout for positions with no world coordinate at all -- architecture.md 12.6's
// galaxy/region zoom levels). `kind` selects a cached vector-shape bake (a small
// `RenderTexture2D`, drawn once per kind and tinted per call rather than re-issuing immediate-mode
// draw calls every frame -- features.md 8.2, architecture.md 15.1 finding 20) so a territory blob,
// a fog-of-war ghost, and a hostile contact read as visibly different things, not just different
// colors of the same dot. `zoom` sizes the marker and is where a future cull/substitution belongs
// (architecture.md 15.1 finding 19 -- P5-05 owns the fuller version); today it only skips drawing
// a marker too small to read. NavigationMap (modes/space/ui/) and SensorContacts are the callers.
void DrawMapMarker(const Vec2& screenPosition, Color color, const std::string& label,
                   MapMarkerKind kind, float zoom);

// features.md 9.1's required camera-AABB cull ("objects outside the camera are simulated but not
// drawn") -- true if a world-space circle (`worldPosition`, `worldRadius`) does not intersect the
// camera's visible rectangle. `screenWidth`/`screenHeight` are taken as explicit parameters rather
// than read via GetScreenWidth/Height, the same split SensorContacts.h's ClampToEdge already uses,
// so this stays unit-testable without a live window; real callers below pass the live values.
bool IsBodyCulled(const Vec2& worldPosition, float worldRadius, const CameraView& camera,
                  float screenWidth, float screenHeight);

// True once `worldRadius` at `camera.zoom` would draw smaller than this system's legibility floor
// -- features.md 9.1 and architecture.md's `BodyKind` comment ("which icon IconRenderer
// substitutes when the body shrinks below a few pixels on zoom-out"). Below this threshold,
// WorldRenderer's DrawWorldBodies skips its true-scale shape and DrawBodyIcon below substitutes a
// fixed-size one instead. Pure arithmetic, unit-testable without a live GL context.
bool NeedsIconSubstitution(float worldRadius, float zoom);

// The placeholder identity color for a world body kind -- WorldRenderer.cpp's true-scale circles
// and DrawBodyIcon's substitute icons both tint from this single mapping, so a planet reads as the
// same blue whether it is drawn at true scale or as a substituted icon.
Color ColorForBodyKind(sr::BodyKind kind);

// Draws a fixed-pixel-size, cached `BodyKind` icon (a small baked `RenderTexture2D`, tinted per
// call from ColorForBodyKind -- features.md 8.2's "runtime template bakes," the same mechanism
// DrawMapMarker above uses for MapMarkerKind) at `worldPosition`, projected to screen space under
// `camera`. This is the substitute WorldRenderer's DrawWorldBodies defers to once
// NeedsIconSubstitution is true for that body. Screen-space; must be called outside
// BeginMode2D/EndMode2D, same as DrawAimReticle/DrawMapMarker.
void DrawBodyIcon(const Vec2& worldPosition, sr::BodyKind kind, const CameraView& camera);

// The screen-space pass over every `WorldBody`: culls off-camera bodies (IsBodyCulled) and calls
// DrawBodyIcon for whichever survive at a substitution-eligible size (NeedsIconSubstitution) --
// WorldRenderer's own DrawWorldBodies applies the identical two predicates to decide which bodies
// it draws at true scale instead, so between the two passes every visible body is drawn exactly
// once, never both or neither. `alpha` is SpaceFlight::InterpolationAlpha, the same fixed-timestep
// blend DrawWorld's own bodies use. Called from SpaceFlight.cpp after DrawWorld's
// BeginMode2D/EndMode2D, alongside DrawAimReticle.
void DrawWorldBodyIcons(const entt::registry& registry, const CameraView& camera, float alpha);

}  // namespace sr::space::render
