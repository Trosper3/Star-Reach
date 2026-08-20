#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/registries/ContentLibrary.h"
#include "modes/space/factories/StationFactory.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/NetworkOwner.h"
#include "shared/components/Physics.h"
#include "shared/components/Research.h"
#include "shared/components/Rig.h"
#include "shared/components/Spawn.h"
#include "shared/rig/CargoView.h"

using Catch::Approx;
using sr::DockingBay;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::FactionRef;
using sr::KnowledgeNetworkId;
using sr::LinearDamping;
using sr::NetworkOwner;
using sr::Propulsion;
using sr::Rig;
using sr::SpawnAnchor;
using sr::StationFacility;
using sr::Wallet;
using sr::core::ContentLibrary;
using sr::space::SystemWorld;
namespace station_factory = sr::space::station_factory;
namespace rig_factory = sr::space::rig_factory;
namespace cargo_view = sr::cargo_view;

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
    // aegis_outpost declares six mounts: core, reactor, dock, turret, hold, bridge.
    CHECK(registry.get<Rig>(result.root).children.size() == 6);
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

TEST_CASE(
    "StationFactory emplaces StationFacility, SpawnAnchor, Wallet and a NetworkOwner naming "
    "the station's own faction",
    "[station_factory]") {
    // architecture.md 13.3 O and 12.30.6: five components with zero producers anywhere. Four of
    // them are hand-emplaced here; the fifth (CargoHold) is checked separately below because it
    // is emergent from a mounted CargoBay module, not hand-emplaced.
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    const auto result = station_factory::Spawn(world, content, OutpostAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();
    CHECK(registry.all_of<StationFacility>(result.root));
    CHECK(registry.all_of<SpawnAnchor>(result.root));
    CHECK(registry.all_of<Wallet>(result.root));
    REQUIRE(registry.all_of<NetworkOwner>(result.root));
    CHECK(registry.get<NetworkOwner>(result.root).network ==
          KnowledgeNetworkId(registry.get<FactionRef>(result.root).id.str()));
}

TEST_CASE("StationFactory's station has a non-zero cargo hold that accepts and refuses deposits",
          "[station_factory]") {
    // The literal "StationFactory needs a non-zero capacity" requirement dissolves per
    // architecture.md's "the hold lives on the bay" note: a station's hold is however many cargo
    // bays it was built with. aegis_outpost's "hold" mount (cargo_bay_i: 4 slots x 250 mass) is
    // what makes this non-zero, not a factory-invented number.
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    const auto result = station_factory::Spawn(world, content, OutpostAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    entt::registry& registry = world.Registry();
    CHECK(cargo_view::Capacity(registry, result.root) == Approx(1000.0f));  // 4 slots x 250.

    const sr::ItemStack stack{sr::ItemKind::Element, "iron", 50, 4.0f};
    CHECK(cargo_view::Deposit(registry, result.root, stack) ==
          cargo_view::DepositResult::Deposited);

    // A deposit that cannot possibly fit (mass alone exceeds every slot's capacity) is refused
    // whole -- HOLD FULL, per the row model architecture.md 3264 describes.
    const sr::ItemStack tooBig{sr::ItemKind::Element, "titanium", 1, 5000.0f};
    CHECK(cargo_view::Deposit(registry, result.root, tooBig) ==
          cargo_view::DepositResult::HoldFull);
}
