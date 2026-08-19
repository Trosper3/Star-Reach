#include "modes/space/ui/AvionicsMenu.h"

#include <raylib.h>

#include "shared/components/Docking.h"
#include "shared/components/Identity.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::avionics_menu {
namespace {

// architecture.md 12.24 step 2: moved off KEY_E now that E is strafe-right (features.md 3.6) --
// the two would otherwise fight over one key every time the player docks while moving.
constexpr int kDockKey = KEY_R;
constexpr float kPromptFontSize = 20.0f;
constexpr float kPromptMarginBottom = 56.0f;  // Sits just above CockpitHud's hull bar.

}  // namespace

// PlayerLocation, not PlayerControlled: architecture.md 12.30.1 makes PlayerLocation the sole
// source of truth today, and nothing derives PlayerControlled yet (P4-01, still open) -- a
// PlayerControlled-gated view here was permanently empty, so pressing R has never done anything
// for anyone, and "[R] DOCK" has never once drawn, regardless of range or faction.
void Update(entt::registry& registry) {
    if (!IsKeyPressed(kDockKey)) {
        return;
    }
    for (auto [entity, location] : registry.view<PlayerLocation>().each()) {
        (void)location;
        if (const DockPrompt* prompt = registry.try_get<DockPrompt>(entity)) {
            registry.emplace_or_replace<DockRequest>(entity, prompt->bay);
        } else if (registry.all_of<Docked>(entity)) {
            registry.emplace_or_replace<UndockRequest>(entity);
        }
        return;  // Exactly one PlayerLocation entity (shared/components/Identity.h).
    }
}

void Draw(const entt::registry& registry) {
    for (auto [entity, location] : registry.view<PlayerLocation>().each()) {
        (void)location;
        const char* label = nullptr;
        if (registry.all_of<DockPrompt>(entity)) {
            label = "[R] DOCK";
        } else if (registry.all_of<Docked>(entity)) {
            label = "[R] UNDOCK";
        }
        if (label == nullptr) {
            return;
        }

        const Font font = GetFontDefault();
        const Vector2 textSize = MeasureTextEx(font, label, kPromptFontSize, 1.0f);
        const float screenWidth = static_cast<float>(GetScreenWidth());
        const float screenHeight = static_cast<float>(GetScreenHeight());
        DrawTextEx(font, label,
                   {(screenWidth - textSize.x) * 0.5f, screenHeight - kPromptMarginBottom},
                   kPromptFontSize, 1.0f, sr::ui::kValueBright);
        return;  // Exactly one PlayerLocation entity.
    }
}

}  // namespace sr::space::ui::avionics_menu
