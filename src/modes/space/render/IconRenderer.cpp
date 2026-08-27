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
        return;  // Too small to read at all -- P5-05 owns substitution, this is just the floor.
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

}  // namespace sr::space::render
