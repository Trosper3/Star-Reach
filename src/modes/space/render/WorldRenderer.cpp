#include "modes/space/render/WorldRenderer.h"

#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "modes/space/render/IconRenderer.h"
#include "modes/space/render/LightingPass.h"
#include "shared/blueprints/Taxonomy.h"
#include "shared/components/Combat.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"
#include "shared/math/Vec2.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::render {

std::vector<entt::entity> SortedHardpointsForDraw(const entt::registry& registry) {
    std::vector<entt::entity> hardpoints;
    for (const entt::entity entity :
         registry.view<WorldTransform, PreviousTransform, ShellRole, HitRadius, DrawLayer,
                       LocalTransform>(entt::exclude<Destroyed>)) {
        hardpoints.push_back(entity);
    }
    std::stable_sort(hardpoints.begin(), hardpoints.end(),
                     [&registry](entt::entity a, entt::entity b) {
                         const int layerA = registry.get<DrawLayer>(a).value;
                         const int layerB = registry.get<DrawLayer>(b).value;
                         if (layerA != layerB) {
                             return layerA < layerB;
                         }
                         return registry.get<LocalTransform>(a).offset.y <
                                registry.get<LocalTransform>(b).offset.y;
                     });
    return hardpoints;
}

entt::entity FindChassisHardpoint(const entt::registry& registry, const Rig& rig) {
    for (const entt::entity child : rig.children) {
        const auto* role = registry.try_get<ShellRole>(child);
        if (role != nullptr && role->kind == ShellKind::Chassis &&
            !registry.all_of<Destroyed>(child)) {
            return child;
        }
    }
    return entt::null;
}

namespace {

// No asset pipeline exists yet (architecture.md section 6, deferred): every shape below is a
// flat-colored placeholder, not sprite art. Swapping these for textures later is a WorldRenderer-
// internal change; nothing outside this file reads a shape or a color.

constexpr float kHardpointMinRadius = 3.0f;
constexpr float kProjectileRadius = 2.5f;
constexpr float kProjectileTracerSeconds = 0.05f;

Vector2 ToRaylib(const Vec2& v) {
    return Vector2{v.x, v.y};
}

Vec2 InterpolatedPosition(const WorldTransform& xf, const PreviousTransform& prev, float alpha) {
    return Lerp(prev.position, xf.position, alpha);
}

float InterpolatedRotation(const WorldTransform& xf, const PreviousTransform& prev, float alpha) {
    return prev.rotation + AngleDelta(prev.rotation, xf.rotation) * alpha;
}

Color ColorForShell(ShellKind kind) {
    switch (kind) {
        case ShellKind::Chassis: return LIGHTGRAY;
        case ShellKind::Armor: return GRAY;
        case ShellKind::PowerCell: return YELLOW;
        case ShellKind::Engine: return ORANGE;
        case ShellKind::Weapon: return RED;
        case ShellKind::Shield: return SKYBLUE;
        case ShellKind::Facility: return VIOLET;
    }
    return WHITE;
}

// Scales a placeholder color by LightingPass's per-object brightness (LightingPass.h) -- the
// closest thing to shading this renderer has until a real sprite/shader pipeline exists.
// Channels clip at 255 rather than wrapping, which is what reproduces the "washes toward white
// near the light source" overexposure look without an actual additive blend.
Color ApplyBrightness(Color base, float brightness) {
    const auto Scale = [brightness](unsigned char channel) {
        return static_cast<unsigned char>(
            std::clamp(static_cast<float>(channel) * brightness, 0.0f, 255.0f));
    };
    return Color{Scale(base.r), Scale(base.g), Scale(base.b), base.a};
}

// World bodies: stars, planets, wrecks, drops and asteroids -- one entity, one flat-colored
// circle each, no hardpoints, no hierarchy. Drawn first, sorted by BodyKind, so DrawWorld's
// header comment is a single readable statement of the whole back-to-front order. A body with no
// PreviousTransform (a drop, a wreck: it never moves) draws unblended rather than carrying the
// component purely to satisfy this loop.
struct DrawableBody {
    Vec2 position;
    float radius;
    BodyKind kind;
};

// Off-camera bodies and bodies too small to read at true scale are both skipped here --
// features.md 9.1's required camera-AABB cull and icon substitution (architecture.md's `BodyKind`
// comment). The latter case is not simply omitted: IconRenderer::DrawWorldBodyIcons runs its own
// pass over the same WorldBody entities, after DrawWorld's BeginMode2D/EndMode2D closes, and
// substitutes a fixed-size icon for exactly the bodies this loop skipped for size. Both passes
// share IsBodyCulled/NeedsIconSubstitution so a body is drawn exactly once, never both or neither.
void DrawWorldBodies(const entt::registry& registry, const CameraView& camera, float alpha) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());

    std::vector<DrawableBody> bodies;
    for (auto [entity, body, xf] : registry.view<WorldBody, WorldTransform>().each()) {
        Vec2 position = xf.position;
        if (const auto* prev = registry.try_get<PreviousTransform>(entity)) {
            position = InterpolatedPosition(xf, *prev, alpha);
        }
        if (IsBodyCulled(position, body.radius, camera, screenWidth, screenHeight)) {
            continue;
        }
        if (NeedsIconSubstitution(body.radius, camera.zoom)) {
            continue;
        }
        bodies.push_back(DrawableBody{position, body.radius, body.kind});
    }
    std::stable_sort(bodies.begin(), bodies.end(),
                     [](const DrawableBody& a, const DrawableBody& b) { return a.kind < b.kind; });

    for (const DrawableBody& body : bodies) {
        const Color color =
            ApplyBrightness(ColorForBodyKind(body.kind), LightForObject(registry, body.position));
        DrawCircleV(ToRaylib(body.position), body.radius, color);
    }
}

// Rig roots: a heading line when the rig has visible thrust, nothing otherwise. The hull itself
// is `DrawHardpoints`' per-hardpoint circles below -- those are what ProjectileSystem actually
// tests. An earlier version of this function drew a triangle (or, unpropelled, a disc) sized by
// the broad-phase CollisionRadius, which is the rig's *maximum* reach across every hardpoint; the
// flanks of that shape routinely extended past what any individual hardpoint's HitRadius covered,
// so a shot could visibly cross the drawn hull without touching the tested one (architecture.md
// 13.3 finding AB). features.md section 3.5 settles this: draw exactly what you test, and show
// heading with a marker rather than by shaping the hull.
void DrawShips(const entt::registry& registry, float alpha) {
    for (auto [entity, xf, prev, radius] :
         registry.view<WorldTransform, PreviousTransform, CollisionRadius>(entt::exclude<Destroyed>)
             .each()) {
        if (!HasVisiblePropulsion(registry, entity)) {
            continue;
        }

        const Vec2 position = InterpolatedPosition(xf, prev, alpha);
        const float rotation = InterpolatedRotation(xf, prev, alpha);
        const Color baseColor = registry.all_of<PlayerControlled>(entity) ? SKYBLUE : ORANGE;
        const Color color = ApplyBrightness(baseColor, LightForObject(registry, position));

        const Vec2 nose = position + Rotated(Vec2{radius.value, 0.0f}, rotation);
        DrawLineV(ToRaylib(position), ToRaylib(nose), color);
    }
}

// Hardpoints: one circle per living child, colored by ShellRole so a weapon, engine, or shield
// mount reads apart from bare armor at a glance. Destroyed hardpoints are excluded entirely --
// there is no wreck-layer art yet, and drawing a dead hardpoint identically to a live one would
// hide exactly the information targeted fire is supposed to communicate. Draws in
// SortedHardpointsForDraw's order, features.md section 3.5's ventral-to-overlay stack, so a
// dorsal turret paints over the ventral engine beneath it rather than the reverse.
void DrawHardpoints(const entt::registry& registry, float alpha) {
    for (const entt::entity entity : SortedHardpointsForDraw(registry)) {
        const auto& xf = registry.get<WorldTransform>(entity);
        const auto& prev = registry.get<PreviousTransform>(entity);
        const auto& role = registry.get<ShellRole>(entity);
        const auto& radius = registry.get<HitRadius>(entity);
        const Vec2 position = InterpolatedPosition(xf, prev, alpha);
        const float drawRadius = radius.value > 0.0f ? radius.value : kHardpointMinRadius;
        const Color color =
            ApplyBrightness(ColorForShell(role.kind), LightForObject(registry, position));
        DrawCircleV(ToRaylib(position), drawRadius, color);
    }
}

// features.md 3.5's persistent "charge-dashed coverage loop" -- the other half of the in-world
// shield shimmer alongside DrawShieldImpacts' per-hit flash below, so a live shield reads as an
// actual field around the ship rather than only announcing itself the instant it blocks
// something. Faded solid (no dashing yet -- a later pass) and shaped per ShieldCoverage exactly
// like a real field of that mode would sit: Personal hugs just the shielded hardpoint's own
// shell, Bubble is a plain circle at its coverageRadius, and Conformal follows the rig's main
// chassis mount (ShellKind::Chassis) rather than every hardpoint's own bulk -- an earlier version
// hulled every living hardpoint together, but on a rig with few or small non-chassis mounts (the
// sandbox's shield dummies) that hull barely exceeded the chassis circle DrawHardpoints already
// draws opaque underneath it, reading as "not there" rather than as a field. Following the one
// mount that is always present and is normally the rig's largest reads correctly regardless of
// how many other shells the rig happens to carry. Skipped for a Destroyed or PowerShed generator,
// or one sitting at 0 charge -- an offline or depleted field projects nothing to look at
// (FindCoveringShield's own reasoning, DamageSystem.cpp).
constexpr float kShieldFieldBaseAlpha = 32.0f;  // Out of 255, at full charge.
// Fraction beyond a hardpoint's own shell every mode pads outward by, so the field reads as its
// own ring/disc rather than a faint tint painted almost exactly on top of the opaque hull art
// DrawHardpoints already drew underneath it.
constexpr float kShieldFieldPad = 1.2f;

Color ShieldFieldColor(const Shield& shield) {
    Color color = sr::ui::DamageTypeColor(shield.absorbs);
    const float chargeFraction =
        shield.max > 0.0f ? std::clamp(shield.current / shield.max, 0.0f, 1.0f) : 0.0f;
    color.a = static_cast<unsigned char>(kShieldFieldBaseAlpha * chargeFraction);
    return color;
}

void DrawRingShieldField(const Vec2& position, float radius, Color color) {
    DrawCircleV(ToRaylib(position), radius, color);
    DrawCircleLinesV(ToRaylib(position), radius, color);
}

float PaddedHitRadius(const entt::registry& registry, entt::entity hardpoint) {
    const auto* hitRadius = registry.try_get<HitRadius>(hardpoint);
    const float radius =
        hitRadius != nullptr && hitRadius->value > 0.0f ? hitRadius->value : kHardpointMinRadius;
    return radius * kShieldFieldPad;
}

void DrawConformalShieldField(const entt::registry& registry, entt::entity shieldEntity,
                              float alpha, Color color) {
    const auto* parent = registry.try_get<ParentRig>(shieldEntity);
    const auto* rig = parent != nullptr ? registry.try_get<Rig>(parent->root) : nullptr;
    if (rig == nullptr) {
        return;
    }
    const entt::entity chassis = FindChassisHardpoint(registry, *rig);
    const auto* xf = chassis != entt::null ? registry.try_get<WorldTransform>(chassis) : nullptr;
    const auto* prev =
        chassis != entt::null ? registry.try_get<PreviousTransform>(chassis) : nullptr;
    if (xf == nullptr || prev == nullptr) {
        return;
    }
    DrawRingShieldField(InterpolatedPosition(*xf, *prev, alpha), PaddedHitRadius(registry, chassis),
                        color);
}

void DrawShields(const entt::registry& registry, float alpha) {
    for (auto [shieldEntity, xf, prev, shield] :
         registry
             .view<WorldTransform, PreviousTransform, Shield>(entt::exclude<Destroyed, PowerShed>)
             .each()) {
        if (shield.current <= 0.0f) {
            continue;
        }
        const Color color = ShieldFieldColor(shield);
        const Vec2 position = InterpolatedPosition(xf, prev, alpha);
        if (shield.coverage == ShieldCoverage::Personal) {
            DrawRingShieldField(position, PaddedHitRadius(registry, shieldEntity), color);
        } else if (shield.coverage == ShieldCoverage::Bubble) {
            DrawRingShieldField(position, shield.coverageRadius, color);
        } else {
            DrawConformalShieldField(registry, shieldEntity, alpha, color);
        }
    }
}

// WeaponStats::colorOverride's own comment: a mod or a base-game module can give a weapon a
// signature look without inventing a new DamageType. Falls back to the damage-type default
// whenever a shot/beam carries no override -- every weapon before this field existed, and most
// after it.
Color ResolveColor(DamageType type, const std::optional<ColorRGBA>& colorOverride) {
    if (colorOverride.has_value()) {
        return Color{colorOverride->r, colorOverride->g, colorOverride->b, colorOverride->a};
    }
    return sr::ui::DamageTypeColor(type);
}

// Projectiles: a short tracer along the flight direction plus a bright head, colored by damage
// type (or WeaponStats::colorOverride) so kinetic and energy fire read apart without a HUD.
void DrawProjectiles(const entt::registry& registry, float alpha) {
    for (auto [entity, xf, prev, velocity, projectile] :
         registry.view<WorldTransform, PreviousTransform, Velocity, Projectile>().each()) {
        (void)entity;
        const Vec2 position = InterpolatedPosition(xf, prev, alpha);
        const Color color = ResolveColor(projectile.damageType, projectile.colorOverride);
        // Clamped to distanceTraveled: a shot only 1-2 ticks past the muzzle hasn't existed long
        // enough to have a full kProjectileTracerSeconds' worth of tail, and drawing one anyway
        // overshoots back past the muzzle -- reading as originating from behind the rig that
        // fired it rather than from the mount itself.
        const float tracerLength = std::min(Length(velocity.linear) * kProjectileTracerSeconds,
                                            projectile.distanceTraveled);
        const Vec2 tail = position - Normalized(velocity.linear) * tracerLength;

        DrawLineV(ToRaylib(tail), ToRaylib(position), color);
        DrawCircleV(ToRaylib(position), kProjectileRadius, color);
    }
}

// Beams: a solid line from the hardpoint to wherever it is currently hitting (or its max range,
// if nothing) -- a continuous weapon has no Projectile entity for DrawProjectiles above to pick
// up, per BeamState's own comment (shared/components/Combat.h). Present on a hardpoint only on a
// tick WeaponSystem actually fired it, so this pass simply draws every entity that carries one,
// the same "draw what's live" idiom DrawHardpoints already follows for Destroyed exclusion.
constexpr float kBeamThickness = 2.0f;

// A slight thickness/brightness pulse over wall-clock time so a held beam doesn't read as one
// static line -- purely cosmetic. GetTime() is otherwise forbidden for anything simulation-facing
// (System.h's own comment: "never from GetTime() -- Law 2's fast-forward depends on..."), but that
// rule binds SystemContext-driven gameplay code, not this presentation-only pass: WeaponSystem
// already computed this tick's real damage from ctx.dt before DrawBeams ever runs, so the beam
// simply looking a little different between two replays of the same tick changes no outcome, the
// same way DrawWorld's own alpha-interpolated motion already isn't tick-locked.
constexpr float kBeamPulseRadiansPerSecond = 10.0f;
constexpr float kBeamPulseDepth = 0.15f;  // +/- fraction of base thickness/alpha.

void DrawBeams(const entt::registry& registry, float alpha) {
    const float pulse = 1.0f + kBeamPulseDepth * std::sin(static_cast<float>(GetTime()) *
                                                          kBeamPulseRadiansPerSecond);
    for (auto [hardpoint, xf, prev, beam] :
         registry.view<WorldTransform, PreviousTransform, BeamState>().each()) {
        const Vec2 origin = InterpolatedPosition(xf, prev, alpha);
        Color color = ResolveColor(beam.damageType, beam.colorOverride);
        color.a = static_cast<unsigned char>(
            std::clamp(static_cast<float>(color.a) * pulse, 0.0f, 255.0f));
        DrawLineEx(ToRaylib(origin), ToRaylib(beam.endPoint), kBeamThickness * pulse, color);
    }
}

// features.md 3.5's "in-world shield shimmer" for the moment a shield actually blocks something,
// scoped to a per-hit flash rather than the full persistent charge-dashed coverage loop that
// section also specifies (still unbuilt). Each ShieldCoverage mode gets a genuinely different
// shape, not just a different position, so a screenshot alone tells Personal/Conformal/Bubble
// apart (ShieldImpactFlash's own comment has the full per-mode position rationale): Personal
// rings the hit hardpoint's own shell (hardpointRadius) since that housing itself is what took
// the hit; Conformal bursts as a filled circle right at the hull contact point, since the whole
// rig's field responded rather than one housing's; Bubble ripples outward as a growing ring from
// the point on its own perimeter the shot actually crossed. All three fade together with
// `fraction` and share the same DamageTypeColor-by-absorbed-type rule (DamageTypeColor: "one
// definition, three readers" -- this is the third, alongside projectiles and beams). No
// interpolation by render alpha: the flash's own worldPosition is already fixed for its short
// lifetime (see that struct's comment on why).
constexpr float kShieldImpactRingThickness = 2.0f;
constexpr float kShieldImpactPersonalPulse = 0.3f;  // +/- fraction of hardpointRadius.
constexpr float kShieldImpactConformalMaxRadius = 10.0f;
constexpr float kShieldImpactBubbleMinRadius = 4.0f;
constexpr float kShieldImpactBubbleMaxRadius = 16.0f;

void DrawPersonalShieldImpact(const ShieldImpactFlash& flash, float fraction, Color color) {
    const float baseRadius =
        flash.hardpointRadius > 0.0f ? flash.hardpointRadius : kHardpointMinRadius;
    const float outerRadius = baseRadius * (1.0f + kShieldImpactPersonalPulse * (1.0f - fraction));
    DrawRing(ToRaylib(flash.worldPosition),
             std::max(0.0f, outerRadius - kShieldImpactRingThickness), outerRadius, 0.0f, 360.0f,
             24, color);
}

void DrawConformalShieldImpact(const ShieldImpactFlash& flash, float fraction, Color color) {
    DrawCircleV(ToRaylib(flash.worldPosition), kShieldImpactConformalMaxRadius * fraction, color);
}

void DrawBubbleShieldImpact(const ShieldImpactFlash& flash, float fraction, Color color) {
    const float outerRadius =
        kShieldImpactBubbleMinRadius +
        (kShieldImpactBubbleMaxRadius - kShieldImpactBubbleMinRadius) * (1.0f - fraction);
    DrawRing(ToRaylib(flash.worldPosition),
             std::max(0.0f, outerRadius - kShieldImpactRingThickness), outerRadius, 0.0f, 360.0f,
             24, color);
}

void DrawShieldImpacts(const entt::registry& registry) {
    for (auto [entity, flash] : registry.view<ShieldImpactFlash>().each()) {
        (void)entity;
        const float fraction =
            std::clamp(flash.secondsRemaining / kShieldImpactFlashSeconds, 0.0f, 1.0f);
        Color color = sr::ui::DamageTypeColor(flash.type);
        color.a = static_cast<unsigned char>(std::clamp(255.0f * fraction, 0.0f, 255.0f));
        switch (flash.coverage) {
            case ShieldCoverage::Personal: DrawPersonalShieldImpact(flash, fraction, color); break;
            case ShieldCoverage::Conformal:
                DrawConformalShieldImpact(flash, fraction, color);
                break;
            case ShieldCoverage::Bubble: DrawBubbleShieldImpact(flash, fraction, color); break;
        }
    }
}

}  // namespace

bool HasVisiblePropulsion(const entt::registry& registry, entt::entity entity) {
    const auto* propulsion = registry.try_get<Propulsion>(entity);
    return propulsion != nullptr && propulsion->thrustNewtons != 0.0f;
}

void DrawWorld(const SystemWorld& world, const CameraView& camera, float alpha) {
    const Camera2D cam2d{
        Vector2{GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f},
        ToRaylib(camera.target),
        0.0f,
        camera.zoom,
    };

    const entt::registry& registry = world.Registry();

    BeginMode2D(cam2d);
    DrawWorldBodies(registry, camera, alpha);
    DrawShips(registry, alpha);
    DrawHardpoints(registry, alpha);
    DrawShields(registry, alpha);
    DrawProjectiles(registry, alpha);
    DrawBeams(registry, alpha);
    DrawShieldImpacts(registry);
    EndMode2D();
}

}  // namespace sr::space::render
