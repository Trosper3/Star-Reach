#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/registries/ContentLibrary.h"
#include "modes/space/factories/NpcFactory.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"

using sr::core::ContentLibrary;
using sr::space::SystemWorld;
namespace npc_factory = sr::space::npc_factory;

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

npc_factory::SpawnParams ScrapperAt(float x, float y) {
    npc_factory::SpawnParams params;
    params.blueprint = sr::BlueprintId("forgotten_scrapper");
    params.position = {x, y};
    return params;
}

}  // namespace

TEST_CASE("NpcFactory spawns the same rig shape RigFactory would", "[npc_factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    const auto result = npc_factory::Spawn(world, content, ScrapperAt(200.0f, 0.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();
    // forgotten_scrapper declares five mounts: core, reactor, thruster, nose gun, cockpit.
    CHECK(registry.get<sr::Rig>(result.root).children.size() == 5);
    CHECK(registry.get<sr::FactionRef>(result.root).id == sr::FactionId("the_forgotten"));
}

TEST_CASE("An NPC spawns as a live target with no PlayerControlled tag", "[npc_factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    const auto result = npc_factory::Spawn(world, content, ScrapperAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();
    CHECK(registry.all_of<sr::Targetable, sr::Target, sr::SensorRange>(result.root));
    CHECK_FALSE(registry.all_of<sr::PlayerControlled>(result.root));
}

TEST_CASE("An unknown NPC blueprint spawns nothing at all", "[npc_factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    npc_factory::SpawnParams params;
    params.blueprint = sr::BlueprintId("no_such_ship");

    const auto result = npc_factory::Spawn(world, content, params);
    CHECK_FALSE(result.ok());
}
