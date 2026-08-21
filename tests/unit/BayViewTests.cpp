#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/ui/BayView.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"

using sr::BlueprintId;
using sr::BlueprintRef;
using sr::Destroyed;
using sr::Docked;
using sr::DisplayName;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::FactionId;
using sr::FactionRef;
using sr::ParentRig;
using sr::Rig;
using sr::space::ui::bay_view::BayRosterEntry;
using sr::space::ui::bay_view::OwnedVesselAt;
using sr::space::ui::bay_view::Roster;
using sr::space::ui::bay_view::SiblingBays;

namespace {

entt::entity MakeBay(entt::registry& registry, entt::entity station) {
    const entt::entity bay = registry.create();
    registry.emplace<ParentRig>(bay, station);
    registry.emplace<FacilityRef>(bay, FacilityKind::Docking);
    return bay;
}

entt::entity MakeDockedVessel(entt::registry& registry, entt::entity station, entt::entity bay,
                              const char* faction) {
    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, bay);
    registry.emplace<FactionRef>(vessel, FactionId(faction));
    return vessel;
}

}  // namespace

TEST_CASE("Roster lists exactly the vessels whose Docked.bay is thisBay", "[bay-view]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity thisBay = MakeBay(registry, station);
    const entt::entity otherBay = MakeBay(registry, station);

    const entt::entity here = MakeDockedVessel(registry, station, thisBay, "aegis");
    MakeDockedVessel(registry, station, otherBay, "aegis");  // A sibling bay -- must not appear.

    const auto roster = Roster(registry, thisBay, entt::null, FactionId("aegis"));
    REQUIRE(roster.size() == 1);
    CHECK(roster[0].vessel == here);
}

TEST_CASE("Roster marks a same-faction vessel owned", "[bay-view]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity bay = MakeBay(registry, station);
    const entt::entity vessel = MakeDockedVessel(registry, station, bay, "aegis");

    const auto roster = Roster(registry, bay, entt::null, FactionId("aegis"));
    REQUIRE(roster.size() == 1);
    CHECK(roster[0].vessel == vessel);
    CHECK(roster[0].owned);
    CHECK_FALSE(roster[0].occupied);
    CHECK(roster[0].row.value == "BOARD");
}

TEST_CASE("Roster marks a foreign-faction vessel unowned, with no verb offered", "[bay-view]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity bay = MakeBay(registry, station);
    MakeDockedVessel(registry, station, bay, "kore");

    const auto roster = Roster(registry, bay, entt::null, FactionId("aegis"));
    REQUIRE(roster.size() == 1);
    CHECK_FALSE(roster[0].owned);
    CHECK(roster[0].row.value.empty());
}

TEST_CASE("Roster marks the PlayerControlled vessel occupied, showing Launch", "[bay-view]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity bay = MakeBay(registry, station);
    const entt::entity vessel = MakeDockedVessel(registry, station, bay, "aegis");

    const auto roster = Roster(registry, bay, /*playerControlled=*/vessel, FactionId("aegis"));
    REQUIRE(roster.size() == 1);
    CHECK(roster[0].occupied);
    CHECK(roster[0].row.value == "LAUNCH");
}

TEST_CASE("Roster carries the vessel's DisplayName and integrity", "[bay-view]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity bay = MakeBay(registry, station);
    const entt::entity vessel = MakeDockedVessel(registry, station, bay, "aegis");
    registry.emplace<DisplayName>(vessel, "Wayfarer");
    registry.emplace<BlueprintRef>(vessel, BlueprintId("aegis_vanguard"));

    const auto roster = Roster(registry, bay, entt::null, FactionId("aegis"));
    REQUIRE(roster.size() == 1);
    CHECK(roster[0].row.label == "Wayfarer");
    // No living hardpoints -- AggregateHullFraction reads a rig with none as zero, not a crash.
    CHECK(roster[0].row.style.integrity == 0.0f);
}

TEST_CASE("SiblingBays lists every living Docking hardpoint on the station", "[bay-view]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    Rig rig;

    const entt::entity bayA = registry.create();
    registry.emplace<FacilityRef>(bayA, FacilityKind::Docking);
    rig.children.push_back(bayA);

    const entt::entity repair = registry.create();
    registry.emplace<FacilityRef>(repair, FacilityKind::Repair);
    rig.children.push_back(repair);

    const entt::entity bayB = registry.create();
    registry.emplace<FacilityRef>(bayB, FacilityKind::Docking);
    rig.children.push_back(bayB);

    registry.emplace<Rig>(station, std::move(rig));

    const auto bays = SiblingBays(registry, station);
    REQUIRE(bays.size() == 2);
    CHECK(bays[0] == bayA);
    CHECK(bays[1] == bayB);
}

TEST_CASE("SiblingBays excludes a Destroyed docking hardpoint", "[bay-view]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    Rig rig;

    const entt::entity bay = registry.create();
    registry.emplace<FacilityRef>(bay, FacilityKind::Docking);
    registry.emplace<Destroyed>(bay);
    rig.children.push_back(bay);

    registry.emplace<Rig>(station, std::move(rig));

    CHECK(SiblingBays(registry, station).empty());
}

TEST_CASE("OwnedVesselAt finds the player's own docked vessel", "[bay-view]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity bay = MakeBay(registry, station);
    const entt::entity vessel = MakeDockedVessel(registry, station, bay, "aegis");
    MakeDockedVessel(registry, station, bay, "kore");  // Not the player's faction.

    CHECK(OwnedVesselAt(registry, station, FactionId("aegis")) == vessel);
}

TEST_CASE("OwnedVesselAt returns null with no owned vessel docked at the station", "[bay-view]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity bay = MakeBay(registry, station);
    MakeDockedVessel(registry, station, bay, "kore");

    CHECK((OwnedVesselAt(registry, station, FactionId("aegis")) == entt::null));
}

TEST_CASE("OwnedVesselAt ignores an owned vessel docked at a different station", "[bay-view]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity otherStation = registry.create();
    const entt::entity bay = MakeBay(registry, otherStation);
    MakeDockedVessel(registry, otherStation, bay, "aegis");

    CHECK((OwnedVesselAt(registry, station, FactionId("aegis")) == entt::null));
}
