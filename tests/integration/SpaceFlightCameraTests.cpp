#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/diplomacy/Reputation.h"
#include "core/economy/FactionEconomy.h"
#include "core/galaxy/WreckRecord.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "core/registries/ContentLibrary.h"
#include "core/time/FixedTimestep.h"
#include "modes/space/SpaceFlight.h"
#include "shared/components/Identity.h"
#include "shared/components/Spawn.h"
#include "shared/components/Transform.h"
#include "shared/components/Warp.h"

using sr::PlayerLocation;
using sr::RespawnPending;
using sr::SpawnAnchor;
using sr::SystemWarpRequest;
using sr::Vec2;
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

entt::entity FindPlayer(entt::registry& registry) {
    entt::entity player = entt::null;
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        player = entity;
    }
    return player;
}

}  // namespace

TEST_CASE("SpaceFlight's camera target follows the player from the first Update",
          "[spaceflight][camera]") {
    // architecture.md 12.24 step 3: cameraTarget_ is default-constructed and previously never
    // written at all, so the view stayed nailed to the origin regardless of where the player was.
    // Asserted against the player's actual spawn position, not a hardcoded origin --
    // architecture.md 12.36 moved that spawn off the origin (the sun's own position) on purpose.
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
    const Vec2 spawnPosition = registry.get<WorldTransform>(player).position;

    game.Update(0.0f);

    CHECK((game.CameraTarget() == spawnPosition));
}

TEST_CASE("SpaceFlight's camera target tracks the player across a system warp",
          "[spaceflight][camera]") {
    ContentLibrary content = Content();
    FactionEconomy economy;
    WreckLedger wreckLedger;
    KnowledgeStore knowledge;
    DiplomacyMatrix diplomacy;
    Reputation reputation;
    SpaceFlight game(content, economy, wreckLedger, knowledge, diplomacy, reputation);
    game.OnEnter();
    game.Update(0.0f);

    entt::registry& before = game.World().Registry();
    const entt::entity player = FindPlayer(before);
    REQUIRE((player != entt::null));
    before.emplace<SystemWarpRequest>(player, "kepler", Vec2{10.0f, 5.0f}, 0.0f);

    game.Update(0.0f);  // Performs the warp; cameraTarget_ here still reflects "sol".
    game.Update(0.0f);  // The next tick catches the camera up to the arrived player.

    CHECK(game.World().SystemId() == "kepler");
    CHECK((game.CameraTarget() == Vec2{10.0f, 5.0f}));
}

TEST_CASE("SpaceFlight's camera target tracks the player across a respawn",
          "[spaceflight][camera]") {
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

    // SpawnSystem::ResolveRespawns places a RespawnPending rig relative to the nearest
    // SpawnAnchor -- nothing in this test's world carries one yet, so give it one to respawn
    // around, far enough from the origin that a stale, un-updated camera target is obvious.
    const entt::entity anchor = registry.create();
    registry.emplace<WorldTransform>(anchor, Vec2{2000.0f, 0.0f}, 0.0f);
    registry.emplace<SpawnAnchor>(anchor);
    registry.emplace<RespawnPending>(player);

    game.Update(sr::core::kFixedDeltaSeconds);  // One tick: SpawnSystem resolves the respawn.

    const Vec2 respawned = registry.get<WorldTransform>(player).position;
    CHECK_FALSE((respawned == Vec2{0.0f, 0.0f}));
    CHECK((game.CameraTarget() == respawned));
}
