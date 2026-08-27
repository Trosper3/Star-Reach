#include "modes/space/ui/SensorContacts.h"

#include <raylib.h>

#include <algorithm>

#include "modes/space/render/IconRenderer.h"
#include "modes/space/ui/NavigationMap.h"
#include "shared/components/Transform.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::sensor_contacts {

std::vector<Contact> Build(const entt::registry& registry, entt::entity player,
                           const core::diplomacy::DiplomacyMatrix* diplomacy,
                           const core::knowledge::KnowledgeStore* knowledge,
                           const std::string& systemId) {
    std::vector<Contact> contacts;
    for (const entt::entity rig :
         navigation_map::VisibleHostileRigs(registry, player, diplomacy, knowledge, systemId)) {
        contacts.push_back(Contact{rig, registry.get<WorldTransform>(rig).position});
    }
    return contacts;
}

EdgeClampResult ClampToEdge(Vec2 point, Vec2 screenCenter, float screenWidth, float screenHeight,
                            float margin) {
    const float left = margin;
    const float right = screenWidth - margin;
    const float top = margin;
    const float bottom = screenHeight - margin;

    if (point.x >= left && point.x <= right && point.y >= top && point.y <= bottom) {
        return EdgeClampResult{point, true};
    }

    // Smallest positive t along the ray from screenCenter through point that lands on the inset
    // rectangle's boundary -- only the axis the point actually overshoots constrains t on that
    // axis, so an axis point doesn't cross (dx or dy == 0) is left unconstrained.
    const float dx = point.x - screenCenter.x;
    const float dy = point.y - screenCenter.y;
    constexpr float kUnconstrained = 1e9f;
    float tX = kUnconstrained;
    if (dx > 0.0f) {
        tX = (right - screenCenter.x) / dx;
    } else if (dx < 0.0f) {
        tX = (left - screenCenter.x) / dx;
    }
    float tY = kUnconstrained;
    if (dy > 0.0f) {
        tY = (bottom - screenCenter.y) / dy;
    } else if (dy < 0.0f) {
        tY = (top - screenCenter.y) / dy;
    }
    const float t = std::max(0.0f, std::min(tX, tY));
    return EdgeClampResult{Vec2{screenCenter.x + t * dx, screenCenter.y + t * dy}, false};
}

void Draw(std::span<const Contact> contacts, const render::CameraView& camera) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    const Vec2 screenCenter{screenWidth * 0.5f, screenHeight * 0.5f};
    constexpr float kMargin = 24.0f;

    for (const Contact& contact : contacts) {
        const Vec2 projected = render::WorldToScreen(contact.worldPosition, camera);
        const EdgeClampResult clamped =
            ClampToEdge(projected, screenCenter, screenWidth, screenHeight, kMargin);
        if (clamped.pointWasInside) {
            continue;  // Already visible as a world sprite -- features.md 3.10.
        }
        render::DrawMapMarker(clamped.position, sr::ui::kStatusCritical, "");
    }
}

}  // namespace sr::space::ui::sensor_contacts
