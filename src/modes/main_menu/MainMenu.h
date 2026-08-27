#pragma once

#include <array>
#include <cstdint>

#include "modes/IGameMode.h"

namespace sr::engine {
// Forward-declared rather than included: both members below are references, so the definitions
// are only needed in the .cpp. That keeps <raylib.h> (which TextureCache.h/FontCache.h do
// include) out of this header, for the same reason engine/platform/Window.h keeps it out of its
// own.
class TextureCache;
class FontCache;
}  // namespace sr::engine

namespace sr::modes::main_menu {

// The title-screen orchestrator -- the second IGameMode, landing in the same commit as the
// interface itself (architecture.md section 3).
//
// Per section 9 (legacy migration plan), legacy MainMenu.cpp is 1,249 lines carrying a ship
// showcase, a multiplayer connect flow, a faction picker with lore text, and a save picker --
// none of which have a supporting content pipeline or system in this codebase yet (no showcase
// renderer, net/ is deferred, no faction lore content, Save Schema Migration is a separate open
// issue). This ports the one piece all of that state existed to serve: a title screen the player
// can actually get past. The rest grows back incrementally, each addition landing with whatever
// it depends on already built, rather than as a 1,249-line file ported whole and immediately
// over every cap in section 2.2.
//
// Law 6: the only state below is presentation state -- a starfield, and the two flags main()
// polls to route to the next mode.
class MainMenu : public IGameMode {
public:
    // Both caches are owned by main() and handed in, exactly the shape ContentLibrary and
    // FactionEconomy already have there -- deliberately not a global. A globally reachable
    // cache is ambient state every file can touch, which is the "what I need is already in
    // scope" pressure Law 6 exists to remove.
    MainMenu(engine::TextureCache& textures, engine::FontCache& fonts);

    void OnEnter() override;
    void Update(float realDeltaSeconds) override;
    void Draw() const override;
    void OnExit() override;

    // Not part of IGameMode: main() polls these to decide when to switch modes or stop the loop
    // entirely. Orchestrating *which* mode runs next (or whether to keep running at all) is
    // main()'s job, not something every mode implements identically.
    bool ShouldStartGame() const { return startRequested_; }
    bool QuitRequested() const { return quitRequested_; }

    // features.md 1.2: which of New Game's two choices set startRequested_ above. Only meaningful
    // once ShouldStartGame() is true -- main.cpp reads it exactly once, in the same frame, to call
    // SpaceFlight::RequestPlayPrologue before that game's OnEnter() runs.
    bool PlayPrologueRequested() const { return playPrologueRequested_; }

private:
    struct Star {
        float x = 0.0f;
        float y = 0.0f;
        float speed = 0.0f;
        float radius = 0.0f;
    };
    static constexpr int kStarCount = 200;

    // features.md 1.2: SINGLEPLAYER stays SINGLEPLAYER -- clicking it opens a second screen
    // offering Play the Prologue / Skip to the Frontier rather than replacing the title screen's
    // own button. Both are still "new game": there is no save/load flow in this stub yet
    // (P10-02), so this distinction is purely about not flattening the two menus into one list.
    enum class Screen : std::uint8_t {
        Title,
        NewGame,
    };

    engine::TextureCache& textures_;
    engine::FontCache& fonts_;
    std::array<Star, kStarCount> stars_{};

    Screen screen_ = Screen::Title;
    bool startRequested_ = false;
    bool quitRequested_ = false;
    bool playPrologueRequested_ = false;

    void InitStars();
    void UpdateStars(float realDeltaSeconds);
    void DrawBackground() const;
    void DrawStars() const;
};

}  // namespace sr::modes::main_menu
