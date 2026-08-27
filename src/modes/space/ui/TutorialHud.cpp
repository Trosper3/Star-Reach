#include "modes/space/ui/TutorialHud.h"

#include <raylib.h>

#include "shared/components/Identity.h"
#include "shared/components/Tutorial.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::tutorial_hud {
namespace {

// Mnemonic Accept/Decline -- free (CommsPanel.h's own kHailKey comment on KEY_O documents the
// same "not a settled binding, just an unclaimed one" reasoning for this file's KEY_Y/KEY_N).
constexpr int kAcceptKey = KEY_Y;
constexpr int kDeclineKey = KEY_N;

constexpr float kPanelTop = 24.0f;
constexpr float kPanelWidth = 460.0f;
constexpr float kPadding = 12.0f;
constexpr float kLineFontSize = 16.0f;
constexpr float kLineHeight = 20.0f;

entt::entity FindPlayerControlled(const entt::registry& registry) {
    for (auto [entity] : registry.view<PlayerControlled>().each()) {
        return entity;
    }
    return entt::null;
}

// features.md 1.2's four taught beats, in TutorialStep's own order. Done has no banner -- the act
// ends at the anomaly, not at finishing every lesson (TutorialSystem.h's own doc comment).
const char* LessonText(TutorialStep step) {
    switch (step) {
        case TutorialStep::Move: return "LESSON: Fly. Thrust away from your starting position.";
        case TutorialStep::Target: return "LESSON: Target. Select a contact.";
        case TutorialStep::Fire: return "LESSON: Fire. Loose a shot at your target.";
        case TutorialStep::Equip: return "LESSON: Equip. Mount a module from your cargo hold.";
        case TutorialStep::Done: return nullptr;
    }
    return nullptr;
}

}  // namespace

void Update(entt::registry& registry) {
    const entt::entity player = FindPlayerControlled(registry);
    if (player == entt::null || !registry.all_of<TutorialOffer>(player)) {
        return;
    }

    if (IsKeyPressed(kAcceptKey)) {
        registry.remove<TutorialOffer>(player);
        registry.emplace<Tutorial>(player);
    } else if (IsKeyPressed(kDeclineKey)) {
        registry.remove<TutorialOffer>(player);
        registry.emplace<TutorialDeclined>(player);
    }
}

void Draw(const entt::registry& registry) {
    const entt::entity player = FindPlayerControlled(registry);
    if (player == entt::null) {
        return;
    }

    const char* line = nullptr;
    if (registry.all_of<TutorialOffer>(player)) {
        line = "Instruction offered. [Y] Accept  [N] Decline";
    } else if (const auto* tutorial = registry.try_get<Tutorial>(player)) {
        line = LessonText(tutorial->step);
    }
    if (line == nullptr) {
        return;
    }

    const Font font = GetFontDefault();
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const Rectangle bounds{(screenWidth - kPanelWidth) * 0.5f, kPanelTop, kPanelWidth,
                           kPadding * 2.0f + kLineHeight};
    sr::ui::DrawBracketPanel(bounds, sr::ui::kPanelGlass, sr::ui::kPanelChrome);
    DrawTextEx(font, line, {bounds.x + kPadding, bounds.y + kPadding}, kLineFontSize, 1.0f,
               sr::ui::kValueBright);
}

}  // namespace sr::space::ui::tutorial_hud
