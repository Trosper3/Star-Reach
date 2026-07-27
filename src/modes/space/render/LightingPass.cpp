#include "modes/space/render/LightingPass.h"

#include <algorithm>

#include "shared/components/Lighting.h"
#include "shared/components/Transform.h"

namespace sr::space::render {
namespace {

// Same constants' SHAPE as lighting.frag, retuned for a per-object (not per-pixel) signal: no
// N.L term to separately weight, so kDiffuseStrength folds directly into how much brighter the
// near side gets, rather than gating a surface-normal dot product that does not exist here.
constexpr float kAmbient = 0.35f;
constexpr float kDiffuseStrength = 0.9f;
constexpr float kOverexposeBonus = 1.2f;
constexpr float kFalloffK = 5.0f;
constexpr float kOverexposeK = 14.0f;

// Normalized inverse falloff: 1 at t=0 (right at the source), decaying with a 1/x-style shape
// down to EXACTLY 0 at t=1 (the range's true edge) -- ported verbatim from lighting.frag's
// InverseFalloff, since the falloff SHAPE is what reads as "light," independent of whether it
// drives a per-pixel or per-object signal.
float InverseFalloff(float t, float k) {
    const float floorValue = 1.0f / (1.0f + k);
    return (1.0f / (1.0f + t * k) - floorValue) / (1.0f - floorValue);
}

}  // namespace

float ComputeBrightness(float distance, float maxRange) {
    if (maxRange <= 0.0f) {
        return kAmbient;
    }
    const float t = std::clamp(distance / maxRange, 0.0f, 1.0f);
    const float attenuation = InverseFalloff(t, kFalloffK);
    const float overexposure = InverseFalloff(t, kOverexposeK) * kOverexposeBonus;
    return kAmbient + kDiffuseStrength * attenuation + overexposure;
}

float LightForObject(const entt::registry& registry, const Vec2& objectPosition) {
    float brightest = kAmbient;
    for (auto [entity, xf, light] : registry.view<WorldTransform, LightSource>().each()) {
        (void)entity;
        const float distance = Distance(objectPosition, xf.position);
        brightest = std::max(brightest, ComputeBrightness(distance, light.maxRange));
    }
    return brightest;
}

}  // namespace sr::space::render
