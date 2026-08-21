#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/PlayerLocationSystem.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"

using sr::ParentRig;
using sr::PlayerControlled;
using sr::PlayerLocation;
using sr::Rig;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace player_location_system = sr::space::player_location_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

}  // namespace

TEST_CASE("PlayerLocationSystem derives PlayerControlled onto shell itself when it is a root",
          "[player-location-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<PlayerLocation>(root, PlayerLocation{root});

    player_location_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.all_of<PlayerControlled>(root));
    CHECK(registry.view<PlayerControlled>().size() == 1);
}

TEST_CASE(
    "PlayerLocationSystem derives PlayerControlled onto ParentRig::root when shell is a "
    "hardpoint",
    "[player-location-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    const entt::entity hardpoint = registry.create();
    registry.emplace<ParentRig>(hardpoint, ParentRig{root});
    registry.emplace<PlayerLocation>(hardpoint, PlayerLocation{hardpoint});

    player_location_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.all_of<PlayerControlled>(root));
    CHECK_FALSE(registry.all_of<PlayerControlled>(hardpoint));
    CHECK(registry.view<PlayerControlled>().size() == 1);
}

TEST_CASE("PlayerLocationSystem leaves PlayerControlled empty with no PlayerLocation entity",
          "[player-location-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    player_location_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.view<PlayerControlled>().size() == 0);
}

TEST_CASE("PlayerLocationSystem moves PlayerControlled when PlayerLocation moves between ticks",
          "[player-location-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity firstRoot = registry.create();
    registry.emplace<Rig>(firstRoot);
    registry.emplace<PlayerLocation>(firstRoot, PlayerLocation{firstRoot});

    player_location_system::Tick(MakeContext(world, intents, content));
    REQUIRE(registry.all_of<PlayerControlled>(firstRoot));

    const entt::entity secondRoot = registry.create();
    registry.emplace<Rig>(secondRoot);
    registry.remove<PlayerLocation>(firstRoot);
    registry.emplace<PlayerLocation>(secondRoot, PlayerLocation{secondRoot});

    player_location_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<PlayerControlled>(firstRoot));
    REQUIRE(registry.all_of<PlayerControlled>(secondRoot));
    CHECK(registry.view<PlayerControlled>().size() == 1);
}
