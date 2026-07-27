#pragma once

#include <entt/entity/registry.hpp>

#include "shared/math/Vec2.h"

// modes/space/render/ -- LightingPass, alongside WorldRenderer (architecture.md section 3).
//
// The old codebase's lighting (assets/shaders/lighting.frag, ../StarReach2) was a real per-pixel
// GLSL shader: tangent-space normal maps, dynamic point lights, an additive Phong split (ambient
// + kd*(N.L)*attenuation + a close-range overexposure bonus). None of that pipeline exists here
// yet -- there is no texture/shader asset pipeline (architecture.md section 6, deliberately
// deferred), and WorldRenderer draws flat-colored placeholder shapes, not sprites. Porting the
// shader itself would be porting infrastructure nothing yet needs (section 2.4 cuts both ways).
//
// What DOES transfer without a shader: the additive Phong SHAPE. Legacy's own LightingSystem
// already had a "tint fallback" mode for when shader compilation failed -- per-pixel N.L
// replaced with per-OBJECT ("treat the whole object as one point," the same approximation Law 4
// budgets for elsewhere). This is exactly that fallback, promoted to the only implementation,
// expressed as pure functions so the falloff math is unit-testable without a live GL context --
// unlike WorldRenderer/HudTheme's Draw* calls, which need InitWindow to have run first.
namespace sr::space::render {

// L_total = ambient + diffuse, with a close-range overexposure bonus layered on top.
// distance/maxRange are world units; distance >= maxRange returns exactly the ambient floor.
float ComputeBrightness(float distance, float maxRange);

// The brightest applicable LightSource for one world-space position, or the ambient floor alone
// if the registry has no LightSource at all. Multiple sources take the max, not a sum, so two
// overlapping ranges do not double-expose an object sitting between them.
float LightForObject(const entt::registry& registry, const Vec2& objectPosition);

}  // namespace sr::space::render
