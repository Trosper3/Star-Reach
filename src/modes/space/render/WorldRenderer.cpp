#include "modes/space/render/WorldRenderer.h"

#include <raylib.h>

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

// Rig roots: a nose-forward triangle sized by the broad-phase collision radius (Physics.h). The
// player's rig is cyan so it reads distinctly against NPC opposition without needing a HUD marker
// yet -- shared/ui/ has no HUD theme to borrow from (architecture.md section 3, still 📋).
void DrawShips(const entt::registry& registry, float alpha) {
    for (auto [entity, xf, prev, radius] :
         registry.view<WorldTransform, PreviousTransform, CollisionRadius>(entt::exclude<Destroyed>)
             .each()) {
        const Vec2 position = InterpolatedPosition(xf, prev, alpha);
        const float rotation = InterpolatedRotation(xf, prev, alpha);
        const Color color = registry.all_of<PlayerControlled>(entity) ? SKYBLUE : ORANGE;

        const Vec2 nose = position + Rotated(Vec2{radius.value, 0.0f}, rotation);
        const Vec2 left =
            position + Rotated(Vec2{-radius.value * 0.7f, radius.value * 0.6f}, rotation);
        const Vec2 right =
            position + Rotated(Vec2{-radius.value * 0.7f, -radius.value * 0.6f}, rotation);

        DrawTriangle(ToRaylib(nose), ToRaylib(left), ToRaylib(right), color);
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
        DrawCircleV(ToRaylib(position), drawRadius, ColorForShell(role.kind));
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

void DrawWorld(const SystemWorld& world, const CameraView& camera, float alpha) {
    const Camera2D cam2d{
        Vector2{GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f},
        ToRaylib(camera.target),
        0.0f,
        camera.zoom,
    };

    const entt::registry& registry = world.Registry();

    BeginMode2D(cam2d);
    DrawShips(registry, alpha);
    DrawHardpoints(registry, alpha);
    DrawProjectiles(registry, alpha);
    EndMode2D();
}

}  // namespace sr::space::render
