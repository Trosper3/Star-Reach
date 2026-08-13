#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/registries/ContentLibrary.h"
#include "modes/space/factories/StationFactory.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"

using Catch::Approx;
using sr::DockingBay;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::FactionRef;
using sr::LinearDamping;
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
    // aegis_outpost declares four mounts: core, reactor, dock, turret.
    CHECK(registry.get<Rig>(result.root).children.size() == 4);
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
    REQUIRE(registry.all_of<FacilityRef>(dock));
    CHECK(registry.get<FacilityRef>(dock).kind == FacilityKind::Docking);
}

TEST_CASE(
    "StationFactory's mobile: false rig still gets Propulsion and LinearDamping, aggregating to "
    "zero thrust",
    "[station_factory]") {
    // Regression test for architecture.md 12.25: capability is emergent, not authored -- every
    // root gets both components, and a station with no engine hardpoints simply aggregates to
    // zero thrust rather than having the components withheld by its blueprint's mobile flag.
    // Also closes finding J: a bare Velocity/BodyMass with no LinearDamping at all is what let a
    // station inside kSunGravityRange accelerate without bound.
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    const auto result = station_factory::Spawn(world, content, OutpostAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();
    REQUIRE(registry.all_of<Propulsion>(result.root));
    CHECK(registry.get<Propulsion>(result.root).thrustNewtons == Approx(0.0f));
    REQUIRE(registry.all_of<LinearDamping>(result.root));
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
