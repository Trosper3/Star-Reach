#pragma once

#include <entt/entity/registry.hpp>
#include <optional>
#include <string>

#include "modes/space/render/WorldRenderer.h"
#include "shared/math/Vec2.h"
#include "shared/ui/HudTheme.h"

// modes/space/render/ -- IconRenderer, alongside WorldRenderer (architecture.md section 3).
//
// WorldRenderer draws world-space sprites: shapes that pan and scale with the camera, drawn
// inside BeginMode2D/EndMode2D. IconRenderer is the distinct thing the issue names it as -- a
// fixed-pixel-size marker projected FROM world space TO screen space, so it stays legible
// regardless of zoom. Called after WorldRenderer, outside BeginMode2D/EndMode2D.
//
// "Map markers" (galaxy map icons, DiscoverySystem's discovered-system set) have no consumer
// yet -- modes/space/ui/ (a separate, dependent issue) is what will eventually call this for
// that. The aim reticle below is this issue's real, present-day consumer.
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

// A fixed-size dot with an optional label beneath it, at a SCREEN-space position the caller has
// already projected (WorldToScreen above, or a map's own layout for positions with no world
// coordinate at all -- architecture.md 12.6's galaxy/region zoom levels). The map-icon bake that
// issue's Home line names: NavigationMap (modes/space/ui/) is the first consumer, for both
// discovered-system markers (no world position) and sensor-gated hostile blips (projected).
void DrawMapMarker(const Vec2& screenPosition, Color color, const std::string& label);

}  // namespace sr::space::render
