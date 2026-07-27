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

}  // namespace

std::optional<Vec2> TargetWorldPosition(const entt::registry& registry, float alpha) {
    for (auto [entity, target] : registry.view<PlayerControlled, Target>().each()) {
        (void)entity;
        const entt::entity rig = target.rig;
        if (rig == entt::null || !registry.valid(rig)) {
            return std::nullopt;
        }
        if (!registry.all_of<WorldTransform, PreviousTransform>(rig) ||
            registry.all_of<Destroyed>(rig)) {
            return std::nullopt;
        }
        const auto& xf = registry.get<WorldTransform>(rig);
        const auto& prev = registry.get<PreviousTransform>(rig);
        return Lerp(prev.position, xf.position, alpha);
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

void DrawTargetReticle(const entt::registry& registry, const CameraView& camera, float alpha) {
    const std::optional<Vec2> worldPos = TargetWorldPosition(registry, alpha);
    if (!worldPos.has_value()) {
        return;
    }
    const Vec2 screenPos = WorldToScreen(*worldPos, camera);
    const Rectangle bounds{screenPos.x - kReticleHalfSize, screenPos.y - kReticleHalfSize,
                           kReticleHalfSize * 2.0f, kReticleHalfSize * 2.0f};
    sr::ui::DrawBracketPanel(bounds, Color{0, 0, 0, 0}, sr::ui::kStatusCritical, kReticleCorner,
                             kReticleThickness);
}

}  // namespace sr::space::render
