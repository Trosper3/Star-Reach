#include "modes/space/render/IconRenderer.h"

#include <raylib.h>

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

void DrawMapMarker(const Vec2& screenPosition, Color color, const std::string& label) {
    DrawCircle(static_cast<int>(screenPosition.x), static_cast<int>(screenPosition.y),
               kMapMarkerRadius, color);
    if (!label.empty()) {
        DrawText(label.c_str(), static_cast<int>(screenPosition.x - kMapMarkerRadius),
                 static_cast<int>(screenPosition.y + kMapMarkerLabelOffset), 10,
                 sr::ui::kValueBright);
    }
}

}  // namespace sr::space::render
