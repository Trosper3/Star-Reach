#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/registries/ContentLibrary.h"
#include "modes/space/factories/StationFactory.h"
#include "shared/components/Docking.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"

using sr::DockingBay;
using sr::FactionRef;
using sr::Propulsion;
using sr::Rig;
using sr::core::ContentLibrary;
using sr::space::SystemWorld;
namespace station_factory = sr::space::station_factory;
namespace rig_factory = sr::space::rig_factory;

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

station_factory::SpawnParams OutpostAt(float x, float y) {
    station_factory::SpawnParams params;
    params.blueprint = sr::BlueprintId("aegis_outpost");
    params.position = {x, y};
    return params;
}

}  // namespace

TEST_CASE("StationFactory spawns the same rig shape RigFactory would", "[station_factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    const auto result = station_factory::Spawn(world, content, OutpostAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();
    // aegis_outpost declares three mounts: core, reactor, dock.
    CHECK(registry.get<Rig>(result.root).children.size() == 3);
    CHECK(registry.get<FactionRef>(result.root).id == sr::FactionId("aegis_directorate"));
}

TEST_CASE("StationFactory attaches DockingBay to the mount carrying a docking module",
          "[station_factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    const auto result = station_factory::Spawn(world, content, OutpostAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();
    const entt::entity dock =
        rig_factory::FindHardpoint(registry, result.root, sr::MountId("dock"));
    REQUIRE((dock != entt::null));
    CHECK(registry.all_of<DockingBay>(dock));
}

TEST_CASE("StationFactory spawns a rig with no Propulsion, matching mobile: false",
          "[station_factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    const auto result = station_factory::Spawn(world, content, OutpostAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    CHECK_FALSE(world.Registry().all_of<Propulsion>(result.root));
}

TEST_CASE("StationFactory refuses a mobile blueprint", "[station_factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    station_factory::SpawnParams params;
    params.blueprint = sr::BlueprintId("aegis_vanguard");  // mobile: true

    const auto result = station_factory::Spawn(world, content, params);
    CHECK_FALSE(result.ok());
}

TEST_CASE("StationFactory refuses an unknown blueprint", "[station_factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    station_factory::SpawnParams params;
    params.blueprint = sr::BlueprintId("no_such_station");

    const auto result = station_factory::Spawn(world, content, params);
    CHECK_FALSE(result.ok());
}
