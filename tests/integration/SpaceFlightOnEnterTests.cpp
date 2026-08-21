#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/diplomacy/Reputation.h"
#include "core/economy/FactionEconomy.h"
#include "core/galaxy/WreckRecord.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/SpaceFlight.h"
#include "modes/space/systems/PlayerRecordSystem.h"
#include "shared/components/Docking.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Orbit.h"
#include "shared/components/Rig.h"
#include "shared/components/Spawn.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"
#include "shared/rig/CargoView.h"

using sr::Distance;
using sr::DockingBay;
using sr::FactionRef;
using sr::GravityWell;
using sr::PlayerLocation;
using sr::Rig;
using sr::SpawnAnchor;
using sr::Vec2;
using sr::Wallet;
using sr::WorldTransform;
using sr::core::ContentLibrary;
using sr::core::diplomacy::DiplomacyMatrix;
using sr::core::diplomacy::Reputation;
using sr::core::economy::FactionEconomy;
using sr::core::galaxy::WreckLedger;
using sr::core::knowledge::KnowledgeStore;
using sr::space::SpaceFlight;
namespace player_record_system = sr::space::player_record_system;

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

entt::entity FindPlayer(entt::registry& registry) {
    entt::entity player = entt::null;
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        player = entity;
    }
    return player;
}

}  // namespace

TEST_CASE("OnEnter populates one sun, one player, one station and the expected NPC count",
          "[spaceflight][onenter]") {
    ContentLibrary content = Content();
    FactionEconomy economy;
    WreckLedger wreckLedger;
    KnowledgeStore knowledge;
    DiplomacyMatrix diplomacy;
    Reputation reputation;
    SpaceFlight game(content, economy, wreckLedger, knowledge, diplomacy, reputation);

    game.OnEnter();

    entt::registry& registry = game.World().Registry();
    CHECK(std::distance(registry.view<GravityWell>().begin(), registry.view<GravityWell>().end()) ==
          1);
    CHECK(std::distance(registry.view<PlayerLocation>().begin(),
                        registry.view<PlayerLocation>().end()) == 1);
    // The station WorldGen now spawns (architecture.md 12.24 step 4) is the only entity carrying
    // SpawnAnchor.
    CHECK(std::distance(registry.view<SpawnAnchor>().begin(), registry.view<SpawnAnchor>().end()) ==
          1);

    // The station is a Rig without PlayerLocation too, so the band becomes one wider on each end.
    const auto npcs = registry.view<Rig>(entt::exclude<PlayerLocation>);
    const auto npcCount = std::distance(npcs.begin(), npcs.end());
    CHECK(npcCount >= 4);
    CHECK(npcCount <= 6);
}

TEST_CASE("OnEnter twice in a row leaves exactly one of each, not two", "[spaceflight][onenter]") {
    // Regression test for architecture.md 12.29's re-entrancy requirement: returning to the main
    // menu and starting a second game calls OnEnter twice in the same process. Without a world_
    // reset before populating, the second call populates on top of the first -- two suns, two
    // players.
    ContentLibrary content = Content();
    FactionEconomy economy;
    WreckLedger wreckLedger;
    KnowledgeStore knowledge;
    DiplomacyMatrix diplomacy;
    Reputation reputation;
    SpaceFlight game(content, economy, wreckLedger, knowledge, diplomacy, reputation);

    game.OnEnter();
    game.OnEnter();

    entt::registry& registry = game.World().Registry();
    CHECK(std::distance(registry.view<GravityWell>().begin(), registry.view<GravityWell>().end()) ==
          1);
    CHECK(std::distance(registry.view<PlayerLocation>().begin(),
                        registry.view<PlayerLocation>().end()) == 1);
    CHECK(std::distance(registry.view<SpawnAnchor>().begin(), registry.view<SpawnAnchor>().end()) ==
          1);
    // One Wallet for the player, one for the station -- both producers now exist (13.3 O/P).
    CHECK(std::distance(registry.view<Wallet>().begin(), registry.view<Wallet>().end()) == 2);
}

TEST_CASE("The player OnEnter spawns has a cargo hold and a wallet", "[spaceflight][onenter]") {
    ContentLibrary content = Content();
    FactionEconomy economy;
    WreckLedger wreckLedger;
    KnowledgeStore knowledge;
    DiplomacyMatrix diplomacy;
    Reputation reputation;
    SpaceFlight game(content, economy, wreckLedger, knowledge, diplomacy, reputation);

    game.OnEnter();

    entt::registry& registry = game.World().Registry();
    const entt::entity player = FindPlayer(registry);
    REQUIRE((player != entt::null));

    CHECK(registry.all_of<Wallet>(player));
    CHECK(sr::cargo_view::Capacity(registry, player) > 0.0f);

    // architecture.md 12.30.3: OnEnter's first spawn writes the player record alongside the rig,
    // not just the rig's own FactionRef.
    CHECK(player_record_system::FactionOf(registry) == registry.get<FactionRef>(player).id);
}

TEST_CASE("OnEnter spawns the player outside the sun's corona, near the station's docking bay",
          "[spaceflight][onenter]") {
    // architecture.md 12.36 / issue #160: OnEnter used to hardcode Vec2{0, 0} -- the sun's own
    // position (WorldGen.cpp's SpawnSun) -- placing the player inside the corona (1200 units)
    // from the first frame, burning every hardpoint before the player could react.
    ContentLibrary content = Content();
    FactionEconomy economy;
    WreckLedger wreckLedger;
    KnowledgeStore knowledge;
    DiplomacyMatrix diplomacy;
    Reputation reputation;
    SpaceFlight game(content, economy, wreckLedger, knowledge, diplomacy, reputation);

    game.OnEnter();

    entt::registry& registry = game.World().Registry();
    const entt::entity player = FindPlayer(registry);
    REQUIRE((player != entt::null));
    const Vec2 playerPos = registry.get<WorldTransform>(player).position;

    // WorldGen.cpp's kCoronaRange -- not exported, repeated here as a literal the way this file's
    // other cases already pin down world-gen's tuned constants (kNpcBandMin/Max above).
    CHECK(Distance(playerPos, Vec2{0.0f, 0.0f}) > 1200.0f);

    const auto bays = registry.view<DockingBay, WorldTransform>();
    REQUIRE(std::distance(bays.begin(), bays.end()) == 1);
    for (auto [bay, bayXf] : bays.each()) {
        CHECK(Distance(playerPos, bayXf.position) < 300.0f);
    }
}
