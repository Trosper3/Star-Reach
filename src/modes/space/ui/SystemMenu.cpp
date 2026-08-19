#include "modes/space/ui/SystemMenu.h"

#include <raylib.h>

#include "shared/ui/HudTheme.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::system_menu {
namespace {

constexpr float kPanelWidth = 360.0f;
constexpr float kPanelHeight = 220.0f;
constexpr float kButtonWidth = 260.0f;
constexpr float kButtonHeight = 40.0f;
constexpr float kButtonSpacing = 16.0f;
constexpr float kButtonTop = 70.0f;
constexpr float kTitleFontSize = 24.0f;
constexpr float kButtonFontSize = 18.0f;

// System.h's "one legitimate cache" exception: a singleton entity rather than mode-held state
// (Law 6), the CommsLog precedent (CommsSystem.cpp's EnsureLog). Created the first time Esc is
// pressed, not up front.
entt::entity FindOrCreateSingleton(entt::registry& registry) {
    for (auto [entity] : registry.view<SystemMenuStateSingleton>().each()) {
        return entity;
    }
    const entt::entity singleton = registry.create();
    registry.emplace<SystemMenuStateSingleton>(singleton);
    registry.emplace<SystemMenuState>(singleton);
    return singleton;
}

entt::entity FindSingleton(const entt::registry& registry) {
    for (auto [entity] : registry.view<SystemMenuStateSingleton>().each()) {
        return entity;
    }
    return entt::null;
}

Rectangle PanelRect() {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    return {(screenWidth - kPanelWidth) / 2.0f, (screenHeight - kPanelHeight) / 2.0f, kPanelWidth,
            kPanelHeight};
}

// `index` 0 is the "positive" row (Resume / Confirm Quit); 1 is the "negative" row (Quit To Main
// Menu / Cancel) -- the two button rows never coexist (SystemMenuState::quitArmed picks one).
Rectangle ButtonRect(const Rectangle& panel, int index) {
    const float x = panel.x + (panel.width - kButtonWidth) / 2.0f;
    const float y =
        panel.y + kButtonTop + static_cast<float>(index) * (kButtonHeight + kButtonSpacing);
    return {x, y, kButtonWidth, kButtonHeight};
}

}  // namespace

void Update(entt::registry& registry) {
    // The Esc ladder, innermost first (architecture.md 12.29) -- rungs 1-2 (cancel build
    // placement, clear selection) land with P8-01/P8-02; neither system exists yet, so Esc
    // always reaches rung 3/4 here today. Insert their early-returns above this check, in this
    // order, when those systems land -- do not reorder what is already here.
    if (IsKeyPressed(KEY_ESCAPE)) {
        SystemMenuState& state = registry.get<SystemMenuState>(FindOrCreateSingleton(registry));
        if (state.open) {
            state.open = false;
            state.quitArmed = false;
        } else {
            state.open = true;
        }
    }

    const entt::entity singleton = FindSingleton(registry);
    if (singleton == entt::null) {
        return;
    }
    SystemMenuState& state = registry.get<SystemMenuState>(singleton);
    if (!state.open || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return;
    }

    const sr::ui::UiInput input{GetMousePosition(), true, 0.0f};
    const Rectangle panel = PanelRect();
    if (state.quitArmed) {
        if (sr::ui::ButtonClicked(ButtonRect(panel, 0), input)) {
            state.quitConfirmed = true;
        } else if (sr::ui::ButtonClicked(ButtonRect(panel, 1), input)) {
            state.quitArmed = false;
        }
        return;
    }
    if (sr::ui::ButtonClicked(ButtonRect(panel, 0), input)) {
        state.open = false;
    } else if (sr::ui::ButtonClicked(ButtonRect(panel, 1), input)) {
        state.quitArmed = true;
    }
}

void Draw(const entt::registry& registry) {
    const entt::entity singleton = FindSingleton(registry);
    if (singleton == entt::null) {
        return;
    }
    const SystemMenuState& state = registry.get<SystemMenuState>(singleton);
    if (!state.open) {
        return;
    }

    const Rectangle panel = PanelRect();
    sr::ui::DrawBracketPanel(panel, sr::ui::kPanelGlass, sr::ui::kPanelChrome);

    const Font font = GetFontDefault();
    const char* title = state.quitArmed ? "ALL PROGRESS IS LOST" : "SYSTEM MENU";
    const Vector2 titleSize = MeasureTextEx(font, title, kTitleFontSize, 1.0f);
    DrawTextEx(font, title, {panel.x + (panel.width - titleSize.x) / 2.0f, panel.y + 20.0f},
               kTitleFontSize, 1.0f, sr::ui::kValueBright);

    if (state.quitArmed) {
        sr::ui::DrawChamferedButton(ButtonRect(panel, 0), "CONFIRM QUIT", font, kButtonFontSize,
                                    sr::ui::kPanelGlass, sr::ui::kStatusCritical,
                                    sr::ui::kValueBright);
        sr::ui::DrawChamferedButton(ButtonRect(panel, 1), "CANCEL", font, kButtonFontSize,
                                    sr::ui::kPanelGlass, sr::ui::kPanelChrome,
                                    sr::ui::kValueBright);
    } else {
        sr::ui::DrawChamferedButton(ButtonRect(panel, 0), "RESUME", font, kButtonFontSize,
                                    sr::ui::kPanelGlass, sr::ui::kStatusGood, sr::ui::kValueBright);
        sr::ui::DrawChamferedButton(ButtonRect(panel, 1), "QUIT TO MAIN MENU", font,
                                    kButtonFontSize, sr::ui::kPanelGlass, sr::ui::kStatusCritical,
                                    sr::ui::kValueBright);
    }
}

bool IsOpen(const entt::registry& registry) {
    const entt::entity singleton = FindSingleton(registry);
    return singleton != entt::null && registry.get<SystemMenuState>(singleton).open;
}

bool QuitConfirmed(const entt::registry& registry) {
    const entt::entity singleton = FindSingleton(registry);
    return singleton != entt::null && registry.get<SystemMenuState>(singleton).quitConfirmed;
}

}  // namespace sr::space::ui::system_menu
