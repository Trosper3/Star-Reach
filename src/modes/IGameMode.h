#pragma once

namespace sr::modes {

// The lifecycle every mode implements (architecture.md section 3's master directory blueprint;
// section 11.8 names the rule this file follows: "IGameMode.h lands with the second mode, not
// before" -- modes/main_menu/ is that second mode alongside modes/space/, in this same commit).
//
// Draw() takes no parameter on purpose. SpaceFlight already keeps Update and Draw split for
// Law 7's reason -- gameplay ticks at a fixed rate (core/time/FixedTimestep.h) while rendering
// runs free and interpolates -- and it already tracks its own interpolation fraction
// (InterpolationAlpha()) rather than main() supplying one; this interface just makes that the
// contract every mode follows, including one like MainMenu that has no simulation tick to
// interpolate at all.
//
// A mode implementing this still obeys Law 6 (no gameplay state on the class itself, a registry
// handle/camera/UI view state and nothing else) and Law 7 (lifecycle + routing + presentation
// math live here; simulation math does not) -- this interface does not relax either law, it just
// names the four methods every orchestrator already had to have.
class IGameMode {
public:
    IGameMode() = default;
    virtual ~IGameMode() = default;

    // Copy is deleted -- a polymorphic base copied through a base-class reference slices the
    // derived object. Move stays defaulted: it only ever runs on the concrete derived type,
    // never through a base pointer, so it does not carry the same footgun.
    IGameMode(const IGameMode&) = delete;
    IGameMode& operator=(const IGameMode&) = delete;
    IGameMode(IGameMode&&) = default;
    IGameMode& operator=(IGameMode&&) = default;

    virtual void OnEnter() = 0;
    virtual void Update(float realDeltaSeconds) = 0;
    virtual void Draw() const = 0;
    virtual void OnExit() = 0;
};

}  // namespace sr::modes
