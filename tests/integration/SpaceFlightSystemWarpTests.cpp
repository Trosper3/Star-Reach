#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/economy/FactionEconomy.h"
#include "core/galaxy/WreckRecord.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/SpaceFlight.h"
#include "modes/space/factories/RigFactory.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Transform.h"
#include "shared/components/Warp.h"

using sr::BlueprintId;
using sr::BlueprintRef;
using sr::CargoHold;
using sr::DeathWreck;
using sr::MaterialStack;
using sr::ModuleId;
using sr::PlayerControlled;
using sr::SystemWarpRequest;
using sr::Vec2;
using sr::Wallet;
using sr::WorldTransform;
using sr::core::ContentLibrary;
using sr::core::economy::FactionEconomy;
using sr::core::galaxy::WreckLedger;
using sr::space::SpaceFlight;
namespace rig_factory = sr::space::rig_factory;

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

// Spawns the player rig directly via RigFactory rather than through SpaceFlight::OnEnter, which
// is still a stub (pre-existing gap, unrelated to system warp -- see its own comment).
entt::entity SpawnPlayer(SpaceFlight& game, const ContentLibrary& content) {
    rig_factory::SpawnParams params;
    params.blueprint = BlueprintId("aegis_vanguard");
    params.position = {0.0f, 0.0f};
    const auto result = rig_factory::Spawn(game.World(), content, params);
    REQUIRE(result.ok());
    game.World().Registry().emplace<PlayerControlled>(result.root);
    return result.root;
}

entt::entity FindPlayer(entt::registry& registry) {
    entt::entity player = entt::null;
    for (auto [entity] : registry.view<PlayerControlled>().each()) {
        player = entity;
    }
    return player;
}

}  // namespace

TEST_CASE("SpaceFlight performs a system warp, preserving blueprint identity and cargo",
          "[spaceflight][warp]") {
    const ContentLibrary content = Content();
    FactionEconomy economy;
    WreckLedger wreckLedger;
    SpaceFlight game(content, economy, wreckLedger);
    game.OnEnter();

    const entt::entity original = SpawnPlayer(game, content);
    game.World().Registry().emplace<CargoHold>(original).materials.push_back({"Fe", 5});
    game.World().Registry().emplace<Wallet>(original, 250);
    game.World().Registry().emplace<SystemWarpRequest>(original, "kepler", Vec2{10.0f, 0.0f}, 0.0f);

    game.Update(0.0f);

    CHECK(game.World().SystemId() == "kepler");

    entt::registry& registry = game.World().Registry();
    const entt::entity arrived = FindPlayer(registry);
    REQUIRE((arrived != entt::null));
    CHECK(registry.get<BlueprintRef>(arrived).id == BlueprintId("aegis_vanguard"));
    CHECK(registry.get<WorldTransform>(arrived).position == Vec2{10.0f, 0.0f});

    REQUIRE(registry.all_of<CargoHold>(arrived));
    const CargoHold& cargo = registry.get<CargoHold>(arrived);
    REQUIRE(cargo.materials.size() == 1);
    CHECK(cargo.materials.front().materialId == "Fe");
    CHECK(cargo.materials.front().quantity == 5);

    REQUIRE(registry.all_of<Wallet>(arrived));
    CHECK(registry.get<Wallet>(arrived).credits == 250);
}

TEST_CASE("SpaceFlight demotes a DeathWreck left behind on system warp", "[spaceflight][warp]") {
    const ContentLibrary content = Content();
    FactionEconomy economy;
    WreckLedger wreckLedger;
    SpaceFlight game(content, economy, wreckLedger);
    game.OnEnter();

    const entt::entity player = SpawnPlayer(game, content);

    const entt::entity wreck = game.World().Registry().create();
    game.World().Registry().emplace<WorldTransform>(wreck, Vec2{500.0f, 500.0f}, 0.0f);
    DeathWreck deathWreck;
    deathWreck.modules.push_back(ModuleId("pulse_cannon_i"));
    deathWreck.materials.push_back(MaterialStack{"Fe", 3});
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
    const ContentLibrary content = Content();
    FactionEconomy economy;
    WreckLedger wreckLedger;
    SpaceFlight game(content, economy, wreckLedger);
    game.OnEnter();

    entt::entity player = SpawnPlayer(game, content);
    const entt::entity wreck = game.World().Registry().create();
    game.World().Registry().emplace<WorldTransform>(wreck, Vec2{500.0f, 500.0f}, 0.0f);
    DeathWreck deathWreck;
    deathWreck.modules.push_back(ModuleId("pulse_cannon_i"));
    deathWreck.materials.push_back(MaterialStack{"Fe", 3});
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
    REQUIRE(promotedWreck.materials.size() == 1);
    CHECK(promotedWreck.materials.front().materialId == "Fe");
}
