#include "modes/space/render/IconRenderer.h"

#include <raylib.h>

#include <unordered_map>

#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::render {
namespace {

constexpr float kReticleHalfSize = 18.0f;
constexpr float kReticleCorner = 8.0f;
constexpr float kReticleThickness = 2.0f;

constexpr float kMapMarkerRadius = 5.0f;
constexpr float kMapMarkerLabelOffset = 10.0f;
constexpr int kMapMarkerBakeSize = 32;
constexpr float kMapMarkerMinVisiblePixels = 1.5f;

constexpr float kBodyIconRadius = 6.0f;  // Fixed screen pixels -- never scales with zoom.
constexpr int kBodyIconBakeSize = 32;
// "a few pixels" -- features.md 9.1. Below this true-scale radius, WorldRenderer's own circle
// reads as noise, not a shape, so DrawBodyIcon substitutes a legible fixed-size one instead.
constexpr float kBodyIconMinTrueScalePixels = 4.0f;

// One small white RenderTexture2D per MapMarkerKind, baked the first time that kind is drawn and
// reused every frame after (features.md 8.2: "runtime template bakes cached into small
// RenderTexture2D's"). Baked white so DrawTexturePro's tint recolors it per call -- every marker
// color a caller needs is a tint, not a second bake. Lives for the process's lifetime; nothing in
// this codebase unloads raylib resources before CloseWindow, and these are a handful of 32x32
// textures, not a per-entity cost.
RenderTexture2D& MapMarkerBake(MapMarkerKind kind) {
    static std::unordered_map<MapMarkerKind, RenderTexture2D> bakes;
    const auto it = bakes.find(kind);
    if (it != bakes.end()) {
        return it->second;
    }

    RenderTexture2D target = LoadRenderTexture(kMapMarkerBakeSize, kMapMarkerBakeSize);
    BeginTextureMode(target);
    ClearBackground(Color{0, 0, 0, 0});
    const Vector2 center{kMapMarkerBakeSize * 0.5f, kMapMarkerBakeSize * 0.5f};
    const float radius = kMapMarkerBakeSize * 0.5f - 2.0f;
    switch (kind) {
        case MapMarkerKind::Territory: DrawPoly(center, 6, radius, 0.0f, WHITE); break;
        case MapMarkerKind::Unknown:
            DrawRing(center, radius * 0.55f, radius, 0.0f, 360.0f, 24, WHITE);
            break;
        case MapMarkerKind::Hostile: DrawCircleV(center, radius, WHITE); break;
    }
    EndTextureMode();

    return bakes.emplace(kind, target).first->second;
}

// One small white RenderTexture2D per BodyKind, baked the first time that kind is drawn and
// reused every frame after -- the identical pattern MapMarkerBake above uses, keyed on the other
// enum this file distinguishes (IconRenderer.h's MapMarkerKind comment). Distinct simple shapes
// per kind, not a shared dot, so a substituted star, planet, and asteroid still read apart at a
// glance.
RenderTexture2D& BodyIconBake(sr::BodyKind kind) {
    static std::unordered_map<sr::BodyKind, RenderTexture2D> bakes;
    const auto it = bakes.find(kind);
    if (it != bakes.end()) {
        return it->second;
    }

    RenderTexture2D target = LoadRenderTexture(kBodyIconBakeSize, kBodyIconBakeSize);
    BeginTextureMode(target);
    ClearBackground(Color{0, 0, 0, 0});
    const Vector2 center{kBodyIconBakeSize * 0.5f, kBodyIconBakeSize * 0.5f};
    const float radius = kBodyIconBakeSize * 0.5f - 2.0f;
    switch (kind) {
        case sr::BodyKind::Star: DrawPoly(center, 8, radius, 0.0f, WHITE); break;
        case sr::BodyKind::Planet: DrawCircleV(center, radius, WHITE); break;
        case sr::BodyKind::Wreck: DrawPoly(center, 3, radius, 0.0f, WHITE); break;
        case sr::BodyKind::Drop: DrawPoly(center, 4, radius, 45.0f, WHITE); break;
        case sr::BodyKind::Asteroid: DrawPoly(center, 5, radius, 0.0f, WHITE); break;
        case sr::BodyKind::Anomaly:
            DrawRing(center, radius * 0.55f, radius, 0.0f, 360.0f, 24, WHITE);
            break;
    }
    EndTextureMode();

    return bakes.emplace(kind, target).first->second;
}

}  // namespace

std::optional<Vec2> AimPointWorldPosition(const entt::registry& registry) {
    // PlayerLocation, not PlayerControlled: architecture.md 12.30.1 makes PlayerLocation the sole
    // source of truth today, and nothing derives PlayerControlled yet (P4-01, still open) -- a
    // PlayerControlled-gated view here is permanently empty, the same trap SpaceFlight.cpp's
    // FindPlayer already documents avoiding.
    for (auto [entity, location, aim] : registry.view<PlayerLocation, AimPoint>().each()) {
        (void)entity;
        (void)location;
        return aim.world;
    }
    return std::nullopt;
}

Vec2 WorldToScreen(const Vec2& worldPosition, const CameraView& camera) {
    const Camera2D cam2d{
        Vector2{GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f},
        Vector2{camera.target.x, camera.target.y},
        0.0f,
        camera.zoom,
    };
    const Vector2 screen = GetWorldToScreen2D(Vector2{worldPosition.x, worldPosition.y}, cam2d);
    return Vec2{screen.x, screen.y};
}

Vec2 ScreenToWorld(const Vec2& screenPosition, const CameraView& camera) {
    const Camera2D cam2d{
        Vector2{GetScreenWidth() * 0.5f, GetScreenHeight() * 0.5f},
        Vector2{camera.target.x, camera.target.y},
        0.0f,
        camera.zoom,
    };
    const Vector2 world = GetScreenToWorld2D(Vector2{screenPosition.x, screenPosition.y}, cam2d);
    return Vec2{world.x, world.y};
}

void DrawAimReticle(const entt::registry& registry, const CameraView& camera) {
    const std::optional<Vec2> worldPos = AimPointWorldPosition(registry);
    if (!worldPos.has_value()) {
        return;
    }
    const Vec2 screenPos = WorldToScreen(*worldPos, camera);
    const Rectangle bounds{screenPos.x - kReticleHalfSize, screenPos.y - kReticleHalfSize,
                           kReticleHalfSize * 2.0f, kReticleHalfSize * 2.0f};
    sr::ui::DrawBracketPanel(bounds, Color{0, 0, 0, 0}, sr::ui::kStatusCritical, kReticleCorner,
                             kReticleThickness);
}

void DrawMapMarker(const Vec2& screenPosition, Color color, const std::string& label,
                   MapMarkerKind kind, float zoom) {
    const float drawnRadius = kMapMarkerRadius * zoom;
    if (drawnRadius < kMapMarkerMinVisiblePixels) {
        // Too small to read at all. P5-05 (IsBodyCulled/NeedsIconSubstitution/DrawBodyIcon below)
        // turned out to scope entirely to WorldRenderer's `BodyKind` objects -- a map marker is
        // already the coarsest aggregate at its zoom level (features.md 8.1), so there is no
        // smaller "substitute" for a territory blob the way a shrinking planet has one. This floor
        // stays the only culling DrawMapMarker needs.
        return;
    }

    const RenderTexture2D& bake = MapMarkerBake(kind);
    // Render textures are stored y-flipped relative to a normal texture -- the negative height is
    // what un-flips it on the way back out.
    const Rectangle source{0.0f, 0.0f, static_cast<float>(bake.texture.width),
                           -static_cast<float>(bake.texture.height)};
    const Rectangle dest{screenPosition.x - drawnRadius, screenPosition.y - drawnRadius,
                         drawnRadius * 2.0f, drawnRadius * 2.0f};
    DrawTexturePro(bake.texture, source, dest, Vector2{0.0f, 0.0f}, 0.0f, color);

    if (!label.empty()) {
        DrawText(label.c_str(), static_cast<int>(screenPosition.x - kMapMarkerRadius),
                 static_cast<int>(screenPosition.y + kMapMarkerLabelOffset), 10,
                 sr::ui::kValueBright);
    }
}

bool IsBodyCulled(const Vec2& worldPosition, float worldRadius, const CameraView& camera,
                  float screenWidth, float screenHeight) {
    const float halfWidth = (screenWidth * 0.5f) / camera.zoom;
    const float halfHeight = (screenHeight * 0.5f) / camera.zoom;
    const float left = camera.target.x - halfWidth - worldRadius;
    const float right = camera.target.x + halfWidth + worldRadius;
    const float top = camera.target.y - halfHeight - worldRadius;
    const float bottom = camera.target.y + halfHeight + worldRadius;
    return worldPosition.x < left || worldPosition.x > right || worldPosition.y < top ||
           worldPosition.y > bottom;
}

bool NeedsIconSubstitution(float worldRadius, float zoom) {
    return worldRadius * zoom < kBodyIconMinTrueScalePixels;
}

Color ColorForBodyKind(sr::BodyKind kind) {
    switch (kind) {
        case sr::BodyKind::Star: return GOLD;
        case sr::BodyKind::Planet: return BLUE;
        case sr::BodyKind::Wreck: return DARKGRAY;
        case sr::BodyKind::Drop: return LIME;
        case sr::BodyKind::Asteroid: return BROWN;
        case sr::BodyKind::Anomaly: return PURPLE;
    }
    return WHITE;
}

void DrawBodyIcon(const Vec2& worldPosition, sr::BodyKind kind, const CameraView& camera) {
    const Vec2 screenPos = WorldToScreen(worldPosition, camera);
    const RenderTexture2D& bake = BodyIconBake(kind);
    // Render textures are stored y-flipped relative to a normal texture -- the negative height is
    // what un-flips it on the way back out (same as DrawMapMarker above).
    const Rectangle source{0.0f, 0.0f, static_cast<float>(bake.texture.width),
                           -static_cast<float>(bake.texture.height)};
    const Rectangle dest{screenPos.x - kBodyIconRadius, screenPos.y - kBodyIconRadius,
                         kBodyIconRadius * 2.0f, kBodyIconRadius * 2.0f};
    DrawTexturePro(bake.texture, source, dest, Vector2{0.0f, 0.0f}, 0.0f, ColorForBodyKind(kind));
}

void DrawWorldBodyIcons(const entt::registry& registry, const CameraView& camera, float alpha) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    for (auto [entity, body, xf] : registry.view<sr::WorldBody, sr::WorldTransform>().each()) {
        Vec2 position = xf.position;
        if (const auto* prev = registry.try_get<sr::PreviousTransform>(entity)) {
            position = Lerp(prev->position, xf.position, alpha);
        }
        if (IsBodyCulled(position, body.radius, camera, screenWidth, screenHeight)) {
            continue;
        }
        if (!NeedsIconSubstitution(body.radius, camera.zoom)) {
            continue;  // WorldRenderer::DrawWorldBodies already drew this one at true scale.
        }
        DrawBodyIcon(position, body.kind, camera);
    }
}

}  // namespace sr::space::render
