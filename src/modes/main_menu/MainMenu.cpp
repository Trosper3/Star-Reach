#include "modes/main_menu/MainMenu.h"

#include <raylib.h>

#include <cstdint>

#include "engine/assets/FontCache.h"
#include "engine/assets/TextureCache.h"
#include "modes/main_menu/MenuFrameMask.h"
#include "shared/ui/HudTheme.h"

namespace sr::modes::main_menu {
namespace {

// Widened from the four-button era's 220.0f: "SKIP TO THE FRONTIER" (features.md 1.2) is longer
// than any label this screen drew before.
constexpr float kButtonWidth = 280.0f;
constexpr float kButtonHeight = 56.0f;
constexpr float kButtonSpacing = 16.0f;
constexpr int kStarMinSpeed = 10;
constexpr int kStarMaxSpeed = 40;
// A star that fails to find a mask-safe spawn cell within this many tries just keeps whatever
// position it last rolled -- menumask's open interior is the overwhelming majority of cells, so
// this is a rare fallback, not the common case (mirrors StarReach2's RandomSafeY).
constexpr int kSafeSpawnAttempts = 8;

// What clicking a button does. Update() only ever acts on the enabled entries below, but the
// action is decided here rather than by comparing button labels, so a label can be reworded
// without touching the click handling.
enum class MenuAction : std::uint8_t {
    kPlayPrologue,
    kSkipToFrontier,
    kMultiplayer,
    kSettings,
    kQuit,
};

struct MenuButtonDef {
    const char* label;
    MenuAction action;
    // Disabled buttons still draw (dimmed) so the player can see what's coming, but Update()
    // skips hit-testing them -- there is nothing to route to yet for multiplayer/settings.
    bool enabled;
};

// features.md 1.2: New Game replaces the old single SINGLEPLAYER button with the two-act
// prologue's own choice -- "Play the prologue" or "Skip to the frontier" -- rather than routing
// through a second confirmation screen this stub has no supporting state machine for yet.
constexpr std::array<MenuButtonDef, 5> kMenuButtons = {{
    {"PLAY THE PROLOGUE", MenuAction::kPlayPrologue, true},
    {"SKIP TO THE FRONTIER", MenuAction::kSkipToFrontier, true},
    {"MULTIPLAYER", MenuAction::kMultiplayer, false},
    {"SETTINGS", MenuAction::kSettings, false},
    {"QUIT", MenuAction::kQuit, true},
}};

// Relative to the TextureCache root (data/base_game/textures/), never an absolute path.
constexpr const char* kBackgroundAsset = "ui/main_menu.png";

// Relative to the FontCache root (data/base_game/fonts/). Only the title/button face is needed
// so far -- Exo2 (the body-text companion this ships with in StarReach2) has no consumer here
// yet, since there's no lore or paragraph text on this screen; it can be added the same way when
// one exists, rather than loading an atlas nothing draws.
constexpr const char* kTitleFontAsset = "Orbitron-VariableFont_wght.ttf";
constexpr int kTitleFontBaseSize = 96;

// Shared between Update (hit-testing the click) and Draw (drawing the button) so the two never
// drift apart -- the window is already open by the time any mode's OnEnter runs (main.cpp calls
// Window::Open before constructing the mode loop), so GetScreenWidth/Height are always valid
// here.
//
// index picks a slot in kMenuButtons; the buttons stack top to bottom starting where the single
// PLAY button used to sit. Adding a fifth entry to kMenuButtons is enough to place it -- nothing
// here needs to change.
Rectangle MenuButtonRect(std::size_t index) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    // 0.58f rather than the four-button era's 0.62f: kMenuButtons grew to five entries with
    // features.md 1.2's prologue/skip-to-frontier split, and starting lower would run the QUIT
    // button off a 900px-tall window.
    const float y =
        screenHeight * 0.58f + static_cast<float>(index) * (kButtonHeight + kButtonSpacing);
    return Rectangle{(screenWidth - kButtonWidth) * 0.5f, y, kButtonWidth, kButtonHeight};
}

// Ported from StarReach2's MainMenu (MenuFrameMask.h): a pixel-accurate bake of ui/main_menu.png's
// border art, so a star vanishes the instant it touches the frame -- every notch and chamfer,
// not just an approximated inset rectangle. Retries a few candidate y's per x so a freshly spawned
// or wrapped star doesn't land back inside the metal on the first roll.
float RandomSafeY(float x, int screenWidth, int screenHeight) {
    const float nx = x / static_cast<float>(screenWidth);
    float y = static_cast<float>(GetRandomValue(0, screenHeight));
    for (int tries = 0;
         tries < kSafeSpawnAttempts && !menumask::IsSafe(nx, y / static_cast<float>(screenHeight));
         ++tries) {
        y = static_cast<float>(GetRandomValue(0, screenHeight));
    }
    return y;
}

}  // namespace

MainMenu::MainMenu(engine::TextureCache& textures, engine::FontCache& fonts)
    : textures_(textures), fonts_(fonts) {}

void MainMenu::OnEnter() {
    startRequested_ = false;
    quitRequested_ = false;
    InitStars();
    // Warmed here rather than left to the first Draw so the decode/upload cost lands during the
    // mode transition instead of on a frame the player is looking at.
    textures_.Get(kBackgroundAsset);
    fonts_.Get(kTitleFontAsset, kTitleFontBaseSize);
}

void MainMenu::Update(float realDeltaSeconds) {
    UpdateStars(realDeltaSeconds);

    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        return;
    }
    const Vector2 mouse = GetMousePosition();
    for (std::size_t i = 0; i < kMenuButtons.size(); ++i) {
        const MenuButtonDef& button = kMenuButtons[i];
        if (!button.enabled || !CheckCollisionPointRec(mouse, MenuButtonRect(i))) {
            continue;
        }
        // kMultiplayer/kSettings can never reach here today -- they're the only disabled entries
        // -- but the switch is the seam a future mode hangs its case off of, instead of this
        // growing into a chain of label string comparisons.
        switch (button.action) {
            case MenuAction::kPlayPrologue:
                startRequested_ = true;
                playPrologueRequested_ = true;
                break;
            case MenuAction::kSkipToFrontier:
                startRequested_ = true;
                playPrologueRequested_ = false;
                break;
            case MenuAction::kQuit: quitRequested_ = true; break;
            case MenuAction::kMultiplayer:
            case MenuAction::kSettings: break;
        }
    }
}

void MainMenu::Draw() const {
    DrawBackground();
    DrawStars();

    const Font& titleFont = fonts_.Get(kTitleFontAsset, kTitleFontBaseSize);

    const char* title = "STAR REACH";
    const float titleFontSize = 64.0f;
    const Vector2 titleSize = MeasureTextEx(titleFont, title, titleFontSize, 1.0f);
    DrawTextEx(titleFont, title,
               Vector2{(static_cast<float>(GetScreenWidth()) - titleSize.x) / 2.0f,
                       static_cast<float>(GetScreenHeight()) / 3.0f},
               titleFontSize, 1.0f, sr::ui::kValueBright);

    for (std::size_t i = 0; i < kMenuButtons.size(); ++i) {
        const MenuButtonDef& button = kMenuButtons[i];
        const Color border = button.enabled ? sr::ui::kPanelChrome : sr::ui::kLabelDim;
        const Color text = button.enabled ? sr::ui::kValueBright : sr::ui::kLabelDim;
        sr::ui::DrawChamferedButton(MenuButtonRect(i), button.label, titleFont, 20.0f,
                                    sr::ui::kPanelGlass, border, text);
    }
}

void MainMenu::OnExit() {
    // ~17 MB of VRAM at this art's resolution, and space flight has no use for it. This is the
    // "big and mode-scoped" case TextureCache::Unload documents; everything smaller can wait for
    // the destructor. The title font atlas is the "everything smaller" case -- kept warm for a
    // possible return to this menu rather than reloaded every time, same as the rest of
    // fonts_/textures_ that OnExit doesn't explicitly unload.
    textures_.Unload(kBackgroundAsset);
}

void MainMenu::InitStars() {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    for (Star& star : stars_) {
        star.x = static_cast<float>(GetRandomValue(0, screenWidth));
        star.y = RandomSafeY(star.x, screenWidth, screenHeight);
        star.speed = static_cast<float>(GetRandomValue(kStarMinSpeed, kStarMaxSpeed));
        star.radius = static_cast<float>(GetRandomValue(1, 2));
    }
}

void MainMenu::UpdateStars(float realDeltaSeconds) {
    const int screenWidth = GetScreenWidth();
    const int screenHeight = GetScreenHeight();
    for (Star& star : stars_) {
        star.x -= star.speed * realDeltaSeconds;
        if (star.x < 0.0f) {
            star.x = static_cast<float>(screenWidth);
            star.y = RandomSafeY(star.x, screenWidth, screenHeight);
        }
    }
}

void MainMenu::DrawBackground() const {
    const Texture2D& background = textures_.Get(kBackgroundAsset);
    const Rectangle source{0.0f, 0.0f, static_cast<float>(background.width),
                           static_cast<float>(background.height)};
    const Rectangle destination{0.0f, 0.0f, static_cast<float>(GetScreenWidth()),
                                static_cast<float>(GetScreenHeight())};
    // Stretched to fill rather than aspect-fitted: this is a frame that has to stay flush to the
    // window edges, and a cover-crop would slice the bezel off. The art is 1.792:1 against the
    // default window's 1.778:1 -- under one percent, invisible on this kind of panel art.
    DrawTexturePro(background, source, destination, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
}

void MainMenu::DrawStars() const {
    // menumask::IsSafe is a pixel-accurate bake of the frame art's border shape (see
    // MenuFrameMask.h), so a star simply isn't drawn wherever that art says it isn't open screen
    // -- replaces the old approximated-inset-rectangle scissor clip with the real border outline,
    // notches and all.
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float screenHeight = static_cast<float>(GetScreenHeight());
    for (const Star& star : stars_) {
        if (!menumask::IsSafe(star.x / screenWidth, star.y / screenHeight)) {
            continue;
        }
        DrawCircleV({star.x, star.y}, star.radius, sr::ui::kLabelDim);
    }
}

}  // namespace sr::modes::main_menu
