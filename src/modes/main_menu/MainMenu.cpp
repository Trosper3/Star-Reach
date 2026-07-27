#include "modes/main_menu/MainMenu.h"

#include <raylib.h>

#include "shared/ui/HudTheme.h"

namespace sr::modes::main_menu {
namespace {

constexpr float kButtonWidth = 220.0f;
constexpr float kButtonHeight = 56.0f;
constexpr int kStarMinSpeed = 10;
constexpr int kStarMaxSpeed = 40;

// Shared between Update (hit-testing the click) and Draw (drawing the button) so the two never
// drift apart -- the window is already open by the time any mode's OnEnter runs (main.cpp calls
// Window::Open before constructing the mode loop), so GetScreenWidth/Height are always valid
// here.
Rectangle PlayButtonRect() {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    return Rectangle{(screenWidth - kButtonWidth) * 0.5f, screenHeight * 0.62f, kButtonWidth,
                     kButtonHeight};
}

}  // namespace

void MainMenu::OnEnter() {
    startRequested_ = false;
    InitStars();
}

void MainMenu::Update(float realDeltaSeconds) {
    UpdateStars(realDeltaSeconds);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        CheckCollisionPointRec(GetMousePosition(), PlayButtonRect())) {
        startRequested_ = true;
    }
}

void MainMenu::Draw() const {
    DrawStars();

    const char* title = "STAR REACH";
    const int titleFontSize = 64;
    const int titleWidth = MeasureText(title, titleFontSize);
    DrawText(title, (GetScreenWidth() - titleWidth) / 2, GetScreenHeight() / 3, titleFontSize,
             sr::ui::kValueBright);

    sr::ui::DrawChamferedButton(PlayButtonRect(), "PLAY", GetFontDefault(), 24.0f,
                                sr::ui::kPanelGlass, sr::ui::kPanelChrome, sr::ui::kValueBright);
}

void MainMenu::OnExit() {}

void MainMenu::InitStars() {
    for (Star& star : stars_) {
        star.x = static_cast<float>(GetRandomValue(0, GetScreenWidth()));
        star.y = static_cast<float>(GetRandomValue(0, GetScreenHeight()));
        star.speed = static_cast<float>(GetRandomValue(kStarMinSpeed, kStarMaxSpeed));
        star.radius = static_cast<float>(GetRandomValue(1, 2));
    }
}

void MainMenu::UpdateStars(float realDeltaSeconds) {
    const float screenHeight = static_cast<float>(GetScreenHeight());
    for (Star& star : stars_) {
        star.y += star.speed * realDeltaSeconds;
        if (star.y > screenHeight) {
            star.y = 0.0f;
            star.x = static_cast<float>(GetRandomValue(0, GetScreenWidth()));
        }
    }
}

void MainMenu::DrawStars() const {
    for (const Star& star : stars_) {
        DrawCircleV({star.x, star.y}, star.radius, sr::ui::kLabelDim);
    }
}

}  // namespace sr::modes::main_menu
