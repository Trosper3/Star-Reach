#include "modes/space/ui/CockpitHud.h"

#include <raylib.h>

#include "modes/space/ui/StatusProjection.h"
#include "shared/components/Identity.h"
#include "shared/rig/ModuleAttachment.h"

namespace sr::space::ui::cockpit_hud {
namespace {

// Placeholder placement/size: features.md 3.10's bottom band (a separate issue) is what actually
// sizes and positions this against the rest of the flight HUD -- "its diameter sets the band
// height." Until that lands, this keeps the projection where the old flat bar sat.
constexpr float kDiameter = 140.0f;
constexpr float kMargin = 24.0f;

}  // namespace

float AggregateHullFraction(const entt::registry& registry, entt::entity rigRoot) {
    return rig_attachment::AggregateStructuralIntegrity(registry, rigRoot);
}

void Draw(const entt::registry& registry) {
    for (auto [entity] : registry.view<PlayerControlled>().each()) {
        const float screenHeight = static_cast<float>(GetScreenHeight());
        const float radius = kDiameter * 0.5f;
        const Vec2 center{kMargin + radius, screenHeight - kMargin - radius};

        const status_projection::Projection projection =
            status_projection::Build(registry, entity, kDiameter);
        status_projection::Draw(projection, center, kDiameter);
        return;  // Exactly one PlayerControlled entity (shared/components/Identity.h).
    }
}

}  // namespace sr::space::ui::cockpit_hud
