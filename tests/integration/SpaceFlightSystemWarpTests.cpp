#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/diplomacy/Reputation.h"
#include "core/economy/FactionEconomy.h"
#include "core/galaxy/WreckRecord.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/SpaceFlight.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Transform.h"
#include "shared/components/Warp.h"

using sr::BlueprintId;
using sr::BlueprintRef;
using sr::DeathWreck;
using sr::ElementStack;
using sr::ModuleId;
using sr::PlayerLocation;
using sr::SystemWarpRequest;
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

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

// `OnEnter` itself spawns the player rig now (architecture.md 12.24 step 1); tests locate it by
// `PlayerLocation`, the source-of-truth component that write site emplaces (12.30.1). No test
// spawns a second rig by hand -- that produced two player-shaped entities disagreeing about which
// one `WarpToSystem` should carry across.
entt::entity FindPlayer(entt::registry& registry) {
    entt::entity player = entt::null;
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        player = entity;
    }
    return player;
}

}  // namespace

TEST_CASE("SpaceFlight performs a system warp, preserving blueprint identity and Wallet",
          "[spaceflight][warp]") {
    // Cargo is deliberately NOT asserted here -- architecture.md 12.23 moved CargoHold onto
    // per-bay hardpoints, which RigFactory::Spawn always rebuilds empty from the blueprint.
    // Carrying cargo across a warp is a documented, accepted gap pending P12.31's RigState
    // (SpaceFlight.h's own comment).
    ContentLibrary content = Content();
    FactionEconomy economy;
    WreckLedger wreckLedger;
    KnowledgeStore knowledge;
    DiplomacyMatrix diplomacy;
    Reputation reputation;
    SpaceFlight game(content, economy, wreckLedger, knowledge, diplomacy, reputation);
    game.OnEnter();

    entt::registry& before = game.World().Registry();
    const entt::entity original = FindPlayer(before);
    REQUIRE((original != entt::null));
    before.get<Wallet>(original).credits = 250;
    before.emplace<SystemWarpRequest>(original, "kepler", Vec2{10.0f, 0.0f}, 0.0f);

    game.Update(0.0f);

    CHECK(game.World().SystemId() == "kepler");

    entt::registry& registry = game.World().Registry();
    const entt::entity arrived = FindPlayer(registry);
    REQUIRE((arrived != entt::null));
    CHECK(registry.get<BlueprintRef>(arrived).id == BlueprintId("aegis_vanguard"));
    CHECK(registry.get<WorldTransform>(arrived).position == Vec2{10.0f, 0.0f});

    REQUIRE(registry.all_of<Wallet>(arrived));
    CHECK(registry.get<Wallet>(arrived).credits == 250);
}

TEST_CASE("SpaceFlight demotes a DeathWreck left behind on system warp", "[spaceflight][warp]") {
    ContentLibrary content = Content();
    FactionEconomy economy;
    WreckLedger wreckLedger;
    KnowledgeStore knowledge;
    DiplomacyMatrix diplomacy;
    Reputation reputation;
    SpaceFlight game(content, economy, wreckLedger, knowledge, diplomacy, reputation);
    game.OnEnter();

    const entt::entity player = FindPlayer(game.World().Registry());
    REQUIRE((player != entt::null));

    const entt::entity wreck = game.World().Registry().create();
    game.World().Registry().emplace<WorldTransform>(wreck, Vec2{500.0f, 500.0f}, 0.0f);
    DeathWreck deathWreck;
    deathWreck.modules.push_back(ModuleId("pulse_cannon_i"));
    deathWreck.elements.push_back(ElementStack{"Fe", 3});
    game.World().Registry().emplace<DeathWreck>(wreck, deathWreck);

    game.World().Registry().emplace<SystemWarpRequest>(player, "kepler", Vec2{0.0f, 0.0f}, 0.0f);
    game.Update(0.0f);

    // The wreck did not travel with the player -- it demoted into the ledger, not the new system.
    entt::registry& newRegistry = game.World().Registry();
    CHECK(newRegistry.storage<DeathWreck>().size() == 0);
    CHECK(wreckLedger.IdsForSystem("kepler").empty());
    REQUIRE(wreckLedger.IdsForSystem("sol").size() == 1);
}

TEST_CASE("SpaceFlight promotes a system's demoted wrecks back when the player returns",
          "[spaceflight][warp]") {
    ContentLibrary content = Content();
    FactionEconomy economy;
    WreckLedger wreckLedger;
    KnowledgeStore knowledge;
    DiplomacyMatrix diplomacy;
    Reputation reputation;
    SpaceFlight game(content, economy, wreckLedger, knowledge, diplomacy, reputation);
    game.OnEnter();

    entt::entity player = FindPlayer(game.World().Registry());
    REQUIRE((player != entt::null));
    const entt::entity wreck = game.World().Registry().create();
    game.World().Registry().emplace<WorldTransform>(wreck, Vec2{500.0f, 500.0f}, 0.0f);
    DeathWreck deathWreck;
    deathWreck.modules.push_back(ModuleId("pulse_cannon_i"));
    deathWreck.elements.push_back(ElementStack{"Fe", 3});
    game.World().Registry().emplace<DeathWreck>(wreck, deathWreck);

    // Leave "sol" for "kepler" -- the wreck demotes into the ledger under "sol".
    game.World().Registry().emplace<SystemWarpRequest>(player, "kepler", Vec2{0.0f, 0.0f}, 0.0f);
    game.Update(0.0f);
    REQUIRE(wreckLedger.IdsForSystem("sol").size() == 1);

    // Return to "sol" -- the ledger's "sol" record should promote back into a live entity.
    player = FindPlayer(game.World().Registry());
    REQUIRE((player != entt::null));
    game.World().Registry().emplace<SystemWarpRequest>(player, "sol", Vec2{0.0f, 0.0f}, 0.0f);
    game.Update(0.0f);

    CHECK(wreckLedger.IdsForSystem("sol").empty());
    entt::registry& registry = game.World().Registry();
    entt::entity promoted = entt::null;
    for (const entt::entity entity : registry.view<DeathWreck>()) {
        promoted = entity;
    }
    REQUIRE((promoted != entt::null));
    const DeathWreck& promotedWreck = registry.get<DeathWreck>(promoted);
    REQUIRE(promotedWreck.modules.size() == 1);
    CHECK(promotedWreck.modules.front() == ModuleId("pulse_cannon_i"));
    REQUIRE(promotedWreck.elements.size() == 1);
    CHECK(promotedWreck.elements.front().elementId == "Fe");
}
