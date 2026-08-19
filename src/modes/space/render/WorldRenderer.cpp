#include "modes/space/render/WorldRenderer.h"

#include <raylib.h>
#include <algorithm>
#include <vector>

#include "modes/space/render/LightingPass.h"
#include "shared/blueprints/Taxonomy.h"
#include "shared/components/Combat.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"

namespace sr::space::render {
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

Color ColorForProjectile(DamageType type) {
    return type == DamageType::Energy ? SKYBLUE : YELLOW;
}

Color ColorForBodyKind(BodyKind kind) {
    switch (kind) {
        case BodyKind::Star: return GOLD;
        case BodyKind::Planet: return BLUE;
        case BodyKind::Wreck: return DARKGRAY;
        case BodyKind::Drop: return LIME;
        case BodyKind::Asteroid: return BROWN;
        case BodyKind::Anomaly: return PURPLE;
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

void DrawWorldBodies(const entt::registry& registry, float alpha) {
    std::vector<DrawableBody> bodies;
    for (auto [entity, body, xf] : registry.view<WorldBody, WorldTransform>().each()) {
        Vec2 position = xf.position;
        if (const auto* prev = registry.try_get<PreviousTransform>(entity)) {
            position = InterpolatedPosition(xf, *prev, alpha);
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
// hide exactly the information targeted fire is supposed to communicate.
void DrawHardpoints(const entt::registry& registry, float alpha) {
    for (auto [entity, xf, prev, role, radius] :
         registry
             .view<WorldTransform, PreviousTransform, ShellRole, HitRadius>(
                 entt::exclude<Destroyed>)
             .each()) {
        (void)entity;
        const Vec2 position = InterpolatedPosition(xf, prev, alpha);
        const float drawRadius = radius.value > 0.0f ? radius.value : kHardpointMinRadius;
        const Color color =
            ApplyBrightness(ColorForShell(role.kind), LightForObject(registry, position));
        DrawCircleV(ToRaylib(position), drawRadius, color);
    }
}

// Projectiles: a short tracer along the flight direction plus a bright head, colored by damage
// type so kinetic and energy fire read apart without a HUD.
void DrawProjectiles(const entt::registry& registry, float alpha) {
    for (auto [entity, xf, prev, velocity, projectile] :
         registry.view<WorldTransform, PreviousTransform, Velocity, Projectile>().each()) {
        (void)entity;
        const Vec2 position = InterpolatedPosition(xf, prev, alpha);
        const Color color = ColorForProjectile(projectile.damageType);
        const Vec2 tail = position - velocity.linear * kProjectileTracerSeconds;

        DrawLineV(ToRaylib(tail), ToRaylib(position), color);
        DrawCircleV(ToRaylib(position), kProjectileRadius, color);
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
    DrawWorldBodies(registry, alpha);
    DrawShips(registry, alpha);
    DrawHardpoints(registry, alpha);
    DrawProjectiles(registry, alpha);
    EndMode2D();
}

}  // namespace sr::space::render
