#pragma once

#include <entt/entity/registry.hpp>
#include <optional>

#include "modes/space/render/WorldRenderer.h"
#include "shared/math/Vec2.h"

// modes/space/render/ -- IconRenderer, alongside WorldRenderer (architecture.md section 3).
//
// WorldRenderer draws world-space sprites: shapes that pan and scale with the camera, drawn
// inside BeginMode2D/EndMode2D. IconRenderer is the distinct thing the issue names it as -- a
// fixed-pixel-size marker projected FROM world space TO screen space, so it stays legible
// regardless of zoom. Called after WorldRenderer, outside BeginMode2D/EndMode2D.
//
// "Map markers" (galaxy map icons, DiscoverySystem's discovered-system set) have no consumer
// yet -- modes/space/ui/ (a separate, dependent issue) is what will eventually call this for
// that. The targeting reticle below is this issue's real, present-day consumer: TargetingSystem
// already writes Target, so there is something to point a reticle at right now.
namespace sr::space::render {

// The interpolated world position of the PlayerControlled entity's current Target.rig, or
// nullopt if there is no player, no target, or the target lacks a WorldTransform (it may have
// died the same tick DamageSystem ran). Pure with respect to the registry -- no raylib -- so it
// is unit-testable without a live GL context, unlike WorldToScreen/DrawTargetReticle below.
std::optional<Vec2> TargetWorldPosition(const entt::registry& registry, float alpha);

// Projects a world-space position into screen-space pixels under `camera`, using the identical
// Camera2D construction WorldRenderer::DrawWorld uses internally -- an icon and the sprite it
// marks stay aligned under panning and zoom.
Vec2 WorldToScreen(const Vec2& worldPosition, const CameraView& camera);

// Draws a bracket reticle (sr_shared_ui's DrawBracketPanel, no fill) over the player's current
// target, sized in fixed screen pixels so it does not shrink or grow with zoom. No-op if
// TargetWorldPosition returns nullopt. Must be called between BeginDrawing/EndDrawing, OUTSIDE
// BeginMode2D/EndMode2D -- it does its own world-to-screen projection.
void DrawTargetReticle(const entt::registry& registry, const CameraView& camera, float alpha);

}  // namespace sr::space::render
