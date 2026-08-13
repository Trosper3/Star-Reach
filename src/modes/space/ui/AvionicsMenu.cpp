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

void Update(entt::registry& registry) {
    if (!IsKeyPressed(kDockKey)) {
        return;
    }
    for (auto [entity] : registry.view<PlayerControlled>().each()) {
        if (const DockPrompt* prompt = registry.try_get<DockPrompt>(entity)) {
            registry.emplace_or_replace<DockRequest>(entity, prompt->bay);
        } else if (registry.all_of<Docked>(entity)) {
            registry.emplace_or_replace<UndockRequest>(entity);
        }
        return;  // Exactly one PlayerControlled entity (shared/components/Identity.h).
    }
}

void Draw(const entt::registry& registry) {
    for (auto [entity] : registry.view<PlayerControlled>().each()) {
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
        return;  // Exactly one PlayerControlled entity.
    }
}

}  // namespace sr::space::ui::avionics_menu
