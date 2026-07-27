#pragma once

#include <array>

#include "modes/IGameMode.h"

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
// Law 6: the only state below is presentation state -- a starfield and a "start requested" flag.
class MainMenu : public IGameMode {
public:
    void OnEnter() override;
    void Update(float realDeltaSeconds) override;
    void Draw() const override;
    void OnExit() override;

    // Not part of IGameMode: main() polls this to decide when to switch modes. Orchestrating
    // *which* mode runs next is main()'s job, not something every mode implements identically.
    bool ShouldStartGame() const { return startRequested_; }

private:
    struct Star {
        float x = 0.0f;
        float y = 0.0f;
        float speed = 0.0f;
        float radius = 0.0f;
    };
    static constexpr int kStarCount = 200;
    std::array<Star, kStarCount> stars_{};

    bool startRequested_ = false;

    void InitStars();
    void UpdateStars(float realDeltaSeconds);
    void DrawStars() const;
};

}  // namespace sr::modes::main_menu
