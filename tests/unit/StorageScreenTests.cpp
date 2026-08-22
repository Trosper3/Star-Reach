#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/ui/StorageScreen.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"

using sr::CargoHold;
using sr::Docked;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::FactionId;
using sr::FactionRef;
using sr::ItemKind;
using sr::ItemStack;
using sr::ParentRig;
using sr::PlayerLocation;
using sr::Rig;
using sr::space::ui::storage_screen::ActiveStation;
using sr::space::ui::storage_screen::BuildDepositRequest;
using sr::space::ui::storage_screen::BuildWithdrawRequest;
using sr::space::ui::storage_screen::OwnedVesselAt;
using sr::space::ui::storage_screen::Rows;
using sr::space::ui::storage_screen::StorageRow;

namespace {

entt::entity GiveCargoBay(entt::registry& registry, entt::entity owner, int slotCount,
                          float slotCapacity) {
    const entt::entity bay = registry.create();
    registry.emplace<ParentRig>(bay, owner);
    registry.emplace<CargoHold>(bay, std::vector<ItemStack>{}, slotCount, slotCapacity);
    registry.emplace<Rig>(owner, std::vector<entt::entity>{bay});
    return bay;
}

}  // namespace

TEST_CASE("Rows marks a stack as fitting when the destination has room", "[storage-screen]") {
    entt::registry registry;
    const entt::entity source = registry.create();
    GiveCargoBay(registry, source, 10, 1000.0f);
    sr::cargo_view::Deposit(registry, source, ItemStack{ItemKind::Element, "Fe", 5, 2.0f});

    const entt::entity destination = registry.create();
    GiveCargoBay(registry, destination, 10, 1000.0f);

    const auto rows = Rows(registry, source, destination);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].id == "Fe");
    CHECK(rows[0].quantity == 5);
    CHECK(rows[0].fits);
    CHECK_FALSE(rows[0].row.style.disabled);
    CHECK(rows[0].row.value == "5");
}

TEST_CASE("Rows marks a stack FULL when the destination lacks mass headroom", "[storage-screen]") {
    entt::registry registry;
    const entt::entity source = registry.create();
    GiveCargoBay(registry, source, 10, 1000.0f);
    sr::cargo_view::Deposit(registry, source, ItemStack{ItemKind::Element, "Fe", 5, 2.0f});

    const entt::entity destination = registry.create();
    GiveCargoBay(registry, destination, 1, 1.0f);  // Barely any mass headroom.

    const auto rows = Rows(registry, source, destination);
    REQUIRE(rows.size() == 1);
    CHECK_FALSE(rows[0].fits);
    CHECK(rows[0].row.style.disabled);
    CHECK(rows[0].row.value == "5  FULL");
}

TEST_CASE("BuildDepositRequest carries the row's stack toward the station", "[storage-screen]") {
    StorageRow row;
    row.kind = ItemKind::Element;
    row.id = "Fe";
    row.quantity = 5;

    const auto request = BuildDepositRequest(row);
    CHECK(request.kind == ItemKind::Element);
    CHECK(request.id == "Fe");
    CHECK(request.quantity == 5);
    CHECK(request.toStation);
}

TEST_CASE("BuildWithdrawRequest carries the row's stack toward the vessel", "[storage-screen]") {
    StorageRow row;
    row.kind = ItemKind::Module;
    row.id = "pulse_cannon_i";
    row.quantity = 1;

    const auto request = BuildWithdrawRequest(row);
    CHECK(request.kind == ItemKind::Module);
    CHECK(request.id == "pulse_cannon_i");
    CHECK(request.quantity == 1);
    CHECK_FALSE(request.toStation);
}

TEST_CASE("ActiveStation resolves for an owned, CargoHold-carrying docked station",
          "[storage-screen]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    registry.emplace<FactionRef>(station, FactionId("aegis"));
    GiveCargoBay(registry, station, 10, 1000.0f);

    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, entt::null);
    registry.emplace<PlayerLocation>(vessel, PlayerLocation{vessel});

    CHECK(ActiveStation(registry, FactionId("aegis")) == station);
}

TEST_CASE("ActiveStation resolves while PlayerLocation is on a facility hardpoint (e.g. Bay)",
          "[storage-screen]") {
    // architecture.md 12.30.2 lands the player on a Docking-kind hardpoint by default; Storage
    // must still resolve as a companion panel while that screen is showing.
    entt::registry registry;
    const entt::entity station = registry.create();
    registry.emplace<FactionRef>(station, FactionId("aegis"));
    const entt::entity cargoBay = registry.create();
    registry.emplace<ParentRig>(cargoBay, station);
    registry.emplace<CargoHold>(cargoBay, std::vector<ItemStack>{}, 10, 1000.0f);

    const entt::entity dockingHardpoint = registry.create();
    registry.emplace<ParentRig>(dockingHardpoint, station);
    registry.emplace<FacilityRef>(dockingHardpoint, FacilityKind::Docking);
    registry.emplace<Rig>(station, std::vector<entt::entity>{cargoBay, dockingHardpoint});
    registry.emplace<PlayerLocation>(dockingHardpoint, PlayerLocation{dockingHardpoint});

    CHECK(ActiveStation(registry, FactionId("aegis")) == station);
}

TEST_CASE("ActiveStation is null when the station is not the player's own faction",
          "[storage-screen]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    registry.emplace<FactionRef>(station, FactionId("kore"));
    GiveCargoBay(registry, station, 10, 1000.0f);

    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, entt::null);
    registry.emplace<PlayerLocation>(vessel, PlayerLocation{vessel});

    CHECK((ActiveStation(registry, FactionId("aegis")) == entt::null));
}

TEST_CASE("ActiveStation is null when the station has no CargoHold", "[storage-screen]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    registry.emplace<FactionRef>(station, FactionId("aegis"));
    registry.emplace<Rig>(station);  // No cargo-bay hardpoint.

    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, entt::null);
    registry.emplace<PlayerLocation>(vessel, PlayerLocation{vessel});

    CHECK((ActiveStation(registry, FactionId("aegis")) == entt::null));
}

TEST_CASE("ActiveStation is null while flying", "[storage-screen]") {
    entt::registry registry;
    const entt::entity vessel = registry.create();
    registry.emplace<PlayerLocation>(vessel, PlayerLocation{vessel});

    CHECK((ActiveStation(registry, FactionId("aegis")) == entt::null));
}

TEST_CASE("OwnedVesselAt finds the player's own docked vessel", "[storage-screen]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, entt::null);
    registry.emplace<FactionRef>(vessel, FactionId("aegis"));

    CHECK(OwnedVesselAt(registry, station, FactionId("aegis")) == vessel);
}

TEST_CASE("OwnedVesselAt returns null with no owned vessel docked at the station",
          "[storage-screen]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, entt::null);
    registry.emplace<FactionRef>(vessel, FactionId("kore"));

    CHECK((OwnedVesselAt(registry, station, FactionId("aegis")) == entt::null));
}
