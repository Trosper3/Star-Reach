#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/diplomacy/Reputation.h"
#include "core/economy/FactionEconomy.h"
#include "core/galaxy/WreckRecord.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "core/registries/ContentLibrary.h"
#include "core/time/FixedTimestep.h"
#include "engine/assets/FontCache.h"
#include "modes/space/SpaceFlight.h"
#include "modes/space/ui/SystemMenu.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"

using sr::PlayerLocation;
using sr::Vec2;
using sr::Velocity;
using sr::WorldTransform;
using sr::core::ContentLibrary;
using sr::core::kFixedDeltaSeconds;
using sr::core::diplomacy::DiplomacyMatrix;
using sr::core::diplomacy::Reputation;
using sr::core::economy::FactionEconomy;
using sr::core::galaxy::WreckLedger;
using sr::core::knowledge::KnowledgeStore;
using sr::engine::FontCache;
using sr::space::SpaceFlight;
using sr::space::ui::system_menu::SystemMenuState;
using sr::space::ui::system_menu::SystemMenuStateSingleton;

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

// `OnEnter` itself spawns the player rig (architecture.md 12.24 step 1); located by
// PlayerLocation, the source-of-truth component that write site emplaces (12.30.1) -- same
// helper SpaceFlightSystemWarpTests.cpp uses.
entt::entity FindPlayer(entt::registry& registry) {
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        return entity;
    }
    return entt::null;
}

// Writes SystemMenuState directly rather than driving it through raylib key/mouse polling --
// the same bypass every other request-driven SpaceFlight integration test uses for its own
// producer (SpaceFlightSystemWarpTests.cpp hand-writes SystemWarpRequest instead of flying a rig
// through a jump gate). There is exactly one singleton per registry (system_menu::Update's own
// find-or-create contract), so tests that also call Update() afterward must create it before
// that call ever runs, not race it.
entt::entity SetMenuState(entt::registry& registry, SystemMenuState state) {
    const entt::entity singleton = registry.create();
    registry.emplace<SystemMenuStateSingleton>(singleton);
    registry.emplace<SystemMenuState>(singleton, state);
    return singleton;
}

}  // namespace

TEST_CASE("ShouldReturnToMenu is a latch that does not re-fire", "[spaceflight][system_menu]") {
    ContentLibrary content = Content();
    FactionEconomy economy;
    WreckLedger wreckLedger;
    KnowledgeStore knowledge;
    DiplomacyMatrix diplomacy;
    Reputation reputation;
    FontCache fonts(std::filesystem::path(SR_DATA_DIR) / "fonts");
    SpaceFlight game(content, economy, wreckLedger, knowledge, diplomacy, reputation, fonts);
    game.OnEnter();

    CHECK_FALSE(game.ShouldReturnToMenu());

    SystemMenuState confirmed;
    confirmed.open = true;
    confirmed.quitArmed = true;
    confirmed.quitConfirmed = true;
    SetMenuState(game.World().Registry(), confirmed);
    game.Update(0.0f);
    CHECK(game.ShouldReturnToMenu());

    // architecture.md 12.29: OnEnter() resets the latch, mirroring MainMenu::OnEnter() resetting
    // its own startRequested_/quitRequested_ -- a second game must not immediately bounce back
    // out because of the first one's confirmed quit.
    game.OnEnter();
    CHECK_FALSE(game.ShouldReturnToMenu());
}

TEST_CASE(
    "Opening the system menu pauses the world: no tick runs, and closing it does not "
    "fast-forward through the paused time",
    "[spaceflight][system_menu]") {
    ContentLibrary content = Content();
    FactionEconomy economy;
    WreckLedger wreckLedger;
    KnowledgeStore knowledge;
    DiplomacyMatrix diplomacy;
    Reputation reputation;
    FontCache fonts(std::filesystem::path(SR_DATA_DIR) / "fonts");
    SpaceFlight game(content, economy, wreckLedger, knowledge, diplomacy, reputation, fonts);
    game.OnEnter();

    entt::registry& registry = game.World().Registry();
    const entt::entity player = FindPlayer(registry);
    REQUIRE((player != entt::null));
    // A coasting velocity, not thrust: FlightControls::Poll needs real input this test never
    // provides, but PhysicsSystem integrates whatever Velocity already holds regardless of
    // ThrustInput, so this alone is enough to prove whether a tick ran.
    registry.get<Velocity>(player).linear = Vec2{600.0f, 0.0f};
    const Vec2 start = registry.get<WorldTransform>(player).position;

    SystemMenuState open;
    open.open = true;
    SetMenuState(registry, open);

    // Several frames' worth of real time, each well over one fixed tick -- if any of it banked
    // in FixedTimestep's accumulator while paused, the resume below would fast-forward through
    // all of it at once instead of advancing by a single tick.
    for (int frame = 0; frame < 5; ++frame) {
        game.Update(kFixedDeltaSeconds * 3.0f);
    }
    CHECK(registry.get<WorldTransform>(player).position == start);

    for (auto [entity, state] : registry.view<SystemMenuState>().each()) {
        (void)entity;
        state.open = false;
    }
    game.Update(kFixedDeltaSeconds);

    const Vec2 moved = registry.get<WorldTransform>(player).position - start;
    const float distance = sr::Length(moved);
    // Damping/max-speed clamping only ever reduce the seeded 600 u/s, never raise it, so one
    // 1/60s tick moves at most 10 units -- a burst of the paused frames' banked time would move
    // several times that.
    CHECK(distance > 0.0f);
    CHECK(distance <= 10.01f);
}
