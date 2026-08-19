#include "modes/space/ui/CockpitHud.h"

#include <raylib.h>
#include <algorithm>

#include "shared/components/Identity.h"
#include "shared/rig/ModuleAttachment.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::cockpit_hud {
namespace {

constexpr float kBarWidth = 240.0f;
constexpr float kBarHeight = 22.0f;
constexpr float kMargin = 24.0f;
constexpr float kInset = 3.0f;

// (raw - threshold) / (1 - threshold) -- features.md 3.2's normalized bar: raw 100% shows 100%,
// raw at the structural-failure threshold shows a true 0%, matching the tick a hull actually
// gives way rather than showing a sliver of bar for a ship that is already destroyed.
float NormalizedFraction(float raw) {
    const float span = 1.0f - rig_attachment::kStructuralFailureThreshold;
    if (span <= 0.0f) {
        return raw;
    }
    return std::clamp((raw - rig_attachment::kStructuralFailureThreshold) / span, 0.0f, 1.0f);
}

}  // namespace

float AggregateHullFraction(const entt::registry& registry, entt::entity rigRoot) {
    return rig_attachment::AggregateStructuralIntegrity(registry, rigRoot);
}

void Draw(const entt::registry& registry) {
    for (auto [entity] : registry.view<PlayerControlled>().each()) {
        const float fraction = NormalizedFraction(AggregateHullFraction(registry, entity));
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
