#include "modes/space/ui/CockpitHud.h"

#include <raylib.h>
#include <algorithm>

#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::cockpit_hud {
namespace {

constexpr float kBarWidth = 240.0f;
constexpr float kBarHeight = 22.0f;
constexpr float kMargin = 24.0f;
constexpr float kInset = 3.0f;

}  // namespace

float AggregateHullFraction(const entt::registry& registry, entt::entity rigRoot) {
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr || rig->children.empty()) {
        return 0.0f;
    }

    float current = 0.0f;
    float max = 0.0f;
    for (const entt::entity child : rig->children) {
        if (const Health* health = registry.try_get<Health>(child)) {
            current += health->current;
            max += health->max;
        }
    }
    return max > 0.0f ? current / max : 0.0f;
}

void Draw(const entt::registry& registry) {
    for (auto [entity] : registry.view<PlayerControlled>().each()) {
        const float fraction = std::clamp(AggregateHullFraction(registry, entity), 0.0f, 1.0f);
        const float screenHeight = static_cast<float>(GetScreenHeight());
        const Rectangle bounds{kMargin, screenHeight - kMargin - kBarHeight, kBarWidth, kBarHeight};

        sr::ui::DrawBracketPanel(bounds, sr::ui::kPanelGlass, sr::ui::kPanelChrome);

        const Color fillColor = fraction > 0.5f   ? sr::ui::kStatusGood
                                : fraction > 0.2f ? sr::ui::kStatusCaution
                                                  : sr::ui::kStatusCritical;
        const Rectangle fill{bounds.x + kInset, bounds.y + kInset,
                             (bounds.width - 2.0f * kInset) * fraction,
                             bounds.height - 2.0f * kInset};
        DrawRectangleRec(fill, fillColor);
        return;  // Exactly one PlayerControlled entity (shared/components/Identity.h).
    }
}

}  // namespace sr::space::ui::cockpit_hud
