#pragma once

#include <string>

#include "core/economy/FactionEconomy.h"
#include "core/events/IntentQueue.h"
#include "core/galaxy/WreckRecord.h"
#include "core/registries/ContentLibrary.h"
#include "core/time/FixedTimestep.h"
#include "modes/IGameMode.h"
#include "modes/space/data/SystemWorld.h"
#include "shared/blueprints/Ids.h"
#include "shared/components/Loot.h"
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
    SpaceFlight(const core::ContentLibrary& content, core::economy::FactionEconomy& economy,
                core::galaxy::WreckLedger& wreckLedger);

    void OnEnter() override;

    // Feeds real frame time to the fixed-timestep clock and runs the schedule for each whole
    // step available. Intents are drained after the full schedule has run -- see the ordering
    // rules in SystemSchedule.h.
    //
    // Also checks for a SystemWarpRequest after the schedule runs and, if one is present, performs
    // the system-level handoff (architecture.md section 12.5 / issue #96) -- this can only happen
    // here, never inside a system, since SystemContext carries no pointer back to this class.
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

    // architecture.md 12.29: true once the player has confirmed Quit To Main Menu in the system
    // menu. A latch, mirroring MainMenu::ShouldStartGame() -- main.cpp polls this once per frame
    // and OnEnter() resets it, so starting a later game does not immediately bounce back out.
    bool ShouldReturnToMenu() const { return returnToMenuRequested_; }

    // The raw (un-interpolated) tick position Update() last read off the player's WorldTransform
    // (architecture.md 12.24 step 3). Draw() blends this toward the player's live transform by
    // InterpolationAlpha() each frame rather than storing an already-interpolated value here.
    Vec2 CameraTarget() const { return cameraTarget_; }

private:
    // The blueprint a fresh game starts the player in. No character-selection flow exists yet
    // (pre-existing gap, out of this class's scope) -- this is the one hardcoded starting point.
    static constexpr const char* kStartingBlueprint = "aegis_vanguard";

    // The local human player's stable actor id (architecture.md 12.24 step 2). Single-player has
    // exactly one non-zero actor -- 0 is reserved for "no actor" (shared/blueprints/Ids.h), so 1
    // is it, here and at every FlightControls::Poll call site.
    static constexpr ActorId kLocalPlayerActorId{1};

    // Populates `world_` (already reset to `targetSystemId` by the caller) via
    // `world_gen::PopulateSystem`, then spawns the player's rig via `rig_factory::Spawn` and
    // emplaces `Wallet` + `PlayerLocation` on it -- the shared body `OnEnter` and `WarpToSystem`
    // both need (architecture.md 12.24 step 1). Not `PlayerControlled`: architecture.md 12.30.1
    // makes `PlayerLocation` the sole source of truth and `PlayerControlled` derived from it
    // (P4-01) -- emplacing both here would let the two disagree about where the player is.
    //
    // Seeded from `targetSystemId` alone via `std::hash<std::string>` -- the same placeholder
    // `WarpToSystem` used before this was extracted (architecture.md 12.24 step 1's warning: one
    // bug to fix later, not two), pending §12.17's real galaxy topology.
    void SpawnPlayerInto(const std::string& targetSystemId, const BlueprintId& blueprint,
                         const FactionId& faction, Vec2 spawnPosition, float spawnRotation,
                         Wallet wallet);

    // Tears down `world_` and stands up `targetSystemId`'s in its place: demotes every DeathWreck
    // left behind into wreckLedger_ (#80), re-spawns the player from their BlueprintRef/FactionRef
    // with Wallet carried over, and promotes any wrecks the destination is owed back into
    // entities. Neither cargo (CargoHold now lives per cargo-bay hardpoint, architecture.md
    // 12.23) nor hardpoint damage/refits are preserved -- there is no live-rig-to-blueprint
    // snapshot capability in this codebase yet (P12.31's RigState, a separate, larger piece of
    // work); the player's rig comes back in its pristine, freshly-spawned state every time.
    void WarpToSystem(const std::string& targetSystemId, Vec2 spawnPosition, float spawnRotation);

    SystemWorld world_;
    core::IntentQueue intents_;
    const core::ContentLibrary& content_;
    core::economy::FactionEconomy& economy_;
    core::galaxy::WreckLedger& wreckLedger_;
    core::FixedTimestep clock_;

    // Presentation state, and the only state this class is permitted to own.
    Vec2 cameraTarget_;
    float cameraZoom_ = 1.0f;

    // Mode state (ShouldReturnToMenu()'s backing field), not UI state -- architecture.md 12.29
    // keeps the system menu's own open/closed state on a registry singleton instead.
    bool returnToMenuRequested_ = false;
};

}  // namespace sr::space
