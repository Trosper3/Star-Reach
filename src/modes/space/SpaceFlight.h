#pragma once

#include "core/economy/FactionEconomy.h"
#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "core/time/FixedTimestep.h"
#include "modes/IGameMode.h"
#include "modes/space/data/SystemWorld.h"
#include "shared/math/Vec2.h"

namespace sr::space {

// The space-flight orchestrator.
//
// Law 6 -- THIS CLASS HOLDS NO GAMEPLAY STATE. A registry handle, a camera, UI view state, and
// nothing else. No entity data, no world data, no faction data.
//
// The member cap is 25 and is enforced by tools/ci/check_sizes.py, not by good intentions.
// StarReach2's equivalent header declared 466 members, which is what made appending to it
// cheaper than creating a file every single time for three years. If you find yourself wanting
// to add a member here, the thing you are adding is either a component (put it on an entity) or
// galaxy-level state (put it in core/).
//
// Law 7 -- lifecycle and routing live here. Presentation math (camera interpolation, viewport
// scaling, UI layout) belongs here too. Simulation math -- AI, combat resolution, physics --
// does not; it belongs in systems/.
class SpaceFlight : public modes::IGameMode {
public:
    SpaceFlight(const core::ContentLibrary& content, core::economy::FactionEconomy& economy);

    void OnEnter() override;

    // Feeds real frame time to the fixed-timestep clock and runs the schedule for each whole
    // step available. Intents are drained after the full schedule has run -- see the ordering
    // rules in SystemSchedule.h.
    void Update(float realDeltaSeconds) override;

    // Reads InterpolationAlpha() internally rather than taking it as a parameter (IGameMode's
    // contract) -- the interpolation fraction toward the next tick, which rendering uses to
    // blend PreviousTransform toward WorldTransform so a 144 Hz display shows smooth motion over
    // a 60 Hz simulation.
    void Draw() const override;

    void OnExit() override;

    SystemWorld& World() { return world_; }
    core::IntentQueue& Intents() { return intents_; }
    float InterpolationAlpha() const { return clock_.Alpha(); }

private:
    SystemWorld world_;
    core::IntentQueue intents_;
    const core::ContentLibrary& content_;
    core::economy::FactionEconomy& economy_;
    core::FixedTimestep clock_;

    // Presentation state, and the only state this class is permitted to own.
    Vec2 cameraTarget_;
    float cameraZoom_ = 1.0f;
};

}  // namespace sr::space
