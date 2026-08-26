#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/ui/BridgeView.h"
#include "modes/space/ui/StorageScreen.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
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
using sr::Health;
using sr::ItemKind;
using sr::ItemStack;
using sr::ParentRig;
using sr::PlayerLocation;
using sr::Rig;
using sr::space::ui::bridge_view::BridgeTab;
using sr::space::ui::bridge_view::ScreenId;
using sr::space::ui::bridge_view::SelectTab;
using sr::space::ui::storage_screen::ActiveGaugeStatus;
using sr::space::ui::storage_screen::ActiveStation;
using sr::space::ui::storage_screen::BuildDepositRequest;
using sr::space::ui::storage_screen::BuildWithdrawRequest;
using sr::space::ui::storage_screen::OwnedVesselAt;
using sr::space::ui::storage_screen::ResolveSelectedHold;
using sr::space::ui::storage_screen::Rows;
using sr::space::ui::storage_screen::SiblingHolds;
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
    CHECK(rows[0].row.value == "x5");
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
    CHECK(rows[0].row.value == "x5  FULL");
}

TEST_CASE("BuildDepositRequest carries the row's stack toward the station", "[storage-screen]") {
    StorageRow row;
    row.kind = ItemKind::Element;
    row.id = "Fe";
    row.quantity = 5;

    const auto request = BuildDepositRequest(row, entt::null);
    CHECK(request.kind == ItemKind::Element);
    CHECK(request.id == "Fe");
    CHECK(request.quantity == 5);
    CHECK(request.toStation);
    CHECK((request.targetHold == entt::null));
}

TEST_CASE("BuildWithdrawRequest carries the row's stack toward the vessel", "[storage-screen]") {
    StorageRow row;
    row.kind = ItemKind::Module;
    row.id = "pulse_cannon_i";
    row.quantity = 1;

    const auto request = BuildWithdrawRequest(row, entt::null);
    CHECK(request.kind == ItemKind::Module);
    CHECK(request.id == "pulse_cannon_i");
    CHECK(request.quantity == 1);
    CHECK_FALSE(request.toStation);
    CHECK((request.targetHold == entt::null));
}

TEST_CASE("BuildDepositRequest/BuildWithdrawRequest forward the chosen sibling hold",
          "[storage-screen]") {
    entt::registry registry;
    const entt::entity chosenHold = registry.create();
    StorageRow row;
    row.kind = ItemKind::Element;
    row.id = "Fe";
    row.quantity = 5;

    CHECK(BuildDepositRequest(row, chosenHold).targetHold == chosenHold);
    CHECK(BuildWithdrawRequest(row, chosenHold).targetHold == chosenHold);
}

TEST_CASE("SiblingHolds lists every living CargoHold hardpoint, skipping Destroyed ones",
          "[storage-screen]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity bayA = GiveCargoBay(registry, root, 4, 100.0f);
    // GiveCargoBay overwrites Rig::children, so build the multi-bay rig by hand here.
    const entt::entity bayB = registry.create();
    registry.emplace<ParentRig>(bayB, root);
    registry.emplace<CargoHold>(bayB, std::vector<ItemStack>{}, 4, 100.0f);
    const entt::entity deadBay = registry.create();
    registry.emplace<ParentRig>(deadBay, root);
    registry.emplace<CargoHold>(deadBay, std::vector<ItemStack>{}, 4, 100.0f);
    registry.emplace<sr::Destroyed>(deadBay);
    registry.replace<Rig>(root, std::vector<entt::entity>{bayA, bayB, deadBay});

    const auto siblings = SiblingHolds(registry, root);

    REQUIRE(siblings.size() == 2);
    CHECK(siblings[0] == bayA);
    CHECK(siblings[1] == bayB);
}

TEST_CASE("SiblingHolds is empty for a rig with no CargoHold hardpoint", "[storage-screen]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);

    CHECK(SiblingHolds(registry, root).empty());
}

TEST_CASE("ResolveSelectedHold keeps the stored pick when it is still a living sibling",
          "[storage-screen]") {
    entt::registry registry;
    const entt::entity a = registry.create();
    const entt::entity b = registry.create();

    CHECK(ResolveSelectedHold({a, b}, b) == b);
}

TEST_CASE("ResolveSelectedHold falls back to the first sibling when nothing is stored yet",
          "[storage-screen]") {
    entt::registry registry;
    const entt::entity a = registry.create();
    const entt::entity b = registry.create();

    CHECK(ResolveSelectedHold({a, b}, entt::null) == a);
}

TEST_CASE(
    "ResolveSelectedHold falls back to the first sibling once the stored pick dies or "
    "moves out of view",
    "[storage-screen]") {
    entt::registry registry;
    const entt::entity a = registry.create();
    const entt::entity b = registry.create();
    const entt::entity destroyedElsewhere = registry.create();

    CHECK(ResolveSelectedHold({a, b}, destroyedElsewhere) == a);
}

TEST_CASE("ResolveSelectedHold is null with no siblings at all", "[storage-screen]") {
    CHECK((ResolveSelectedHold({}, entt::null) == entt::null));
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
    // architecture.md 12.30.2 lands the player on a Docking-kind hardpoint by default;
    // ActiveStation is the structural "could Storage apply here" check and stays true regardless --
    // it is Update/Draw's separate bridge_view::IsStorageSelected gate (architecture.md 12.30's
    // frame) that keeps Storage and a hardpoint screen from ever showing full-screen at the same
    // time.
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

TEST_CASE(
    "ActiveGaugeStatus resolves the selected station hold's own label and integrity, not the "
    "vessel's cargo fill",
    "[storage-screen]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    registry.emplace<FactionRef>(station, FactionId("aegis"));
    const entt::entity hold = GiveCargoBay(registry, station, 10, 1000.0f);
    registry.emplace<Health>(hold, Health{50.0f, 100.0f});

    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, entt::null);
    registry.emplace<FactionRef>(vessel, FactionId("aegis"));
    registry.emplace<PlayerLocation>(vessel, PlayerLocation{vessel});

    const std::vector<BridgeTab> tabs{{ScreenId::Storage, entt::null}};
    SelectTab(registry, vessel, tabs, 0);

    const auto status = ActiveGaugeStatus(registry, FactionId("aegis"));
    REQUIRE(status.has_value());
    CHECK(status->label == "STATION HOLD");
    CHECK(status->fraction == 0.5f);
}

TEST_CASE("ActiveGaugeStatus is nullopt while Storage is not the selected tab",
          "[storage-screen]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    registry.emplace<FactionRef>(station, FactionId("aegis"));
    GiveCargoBay(registry, station, 10, 1000.0f);

    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, entt::null);
    registry.emplace<FactionRef>(vessel, FactionId("aegis"));
    registry.emplace<PlayerLocation>(vessel, PlayerLocation{vessel});

    CHECK_FALSE(ActiveGaugeStatus(registry, FactionId("aegis")).has_value());
}
