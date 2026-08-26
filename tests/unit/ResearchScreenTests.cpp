#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/ui/ResearchScreen.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Research.h"
#include "shared/components/Rig.h"

using sr::CargoHold;
using sr::Docked;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::FactionId;
using sr::FactionRef;
using sr::Health;
using sr::ItemKind;
using sr::ItemStack;
using sr::ModuleId;
using sr::MountId;
using sr::MountRef;
using sr::ParentRig;
using sr::PlayerLocation;
using sr::ResearchJob;
using sr::Rig;
using sr::StationFacility;
using sr::core::knowledge::KnowledgeNetwork;
using sr::space::ui::research_screen::ActiveGaugeStatus;
using sr::space::ui::research_screen::Candidates;
using sr::space::ui::research_screen::OwnedVesselAt;
using sr::space::ui::research_screen::QueueRows;

namespace {

entt::entity MakeCargoRequester(entt::registry& registry, std::vector<ItemStack> stacks) {
    const entt::entity requester = registry.create();
    const entt::entity bay = registry.create();
    registry.emplace<CargoHold>(bay, CargoHold{std::move(stacks), 4, 10.0f});
    registry.emplace<Rig>(requester, std::vector<entt::entity>{bay});
    return requester;
}

entt::entity MakeFacility(entt::registry& registry, const MountId& mount, int grade = 1,
                          int capacity = 0) {
    const entt::entity facility = registry.create();
    registry.emplace<FacilityRef>(facility, FacilityKind::Research, grade, capacity);
    registry.emplace<MountRef>(facility, mount);
    return facility;
}

}  // namespace

TEST_CASE("Candidates lists distinct Module ids held in the requester's CargoHold",
          "[research-screen]") {
    entt::registry registry;
    const entt::entity requester =
        MakeCargoRequester(registry, {ItemStack{ItemKind::Module, "pulse_cannon_i", 2, 1.0f},
                                      ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 1.0f},
                                      ItemStack{ItemKind::Element, "iron", 5, 1.0f}});
    const entt::entity station = registry.create();
    const entt::entity facility = MakeFacility(registry, MountId("lab"));

    const auto rows = Candidates(registry, requester, facility, station, /*network=*/nullptr,
                                 /*queuedCount=*/0, /*capacity=*/0);

    REQUIRE(rows.size() == 1);  // Deduped across two Module stacks; the Element stack excluded.
    CHECK(rows[0].item == ModuleId("pulse_cannon_i"));
    CHECK_FALSE(rows[0].row.style.disabled);
}

TEST_CASE("Candidates disables a row already unlocked in the target network", "[research-screen]") {
    entt::registry registry;
    const entt::entity requester =
        MakeCargoRequester(registry, {ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 1.0f}});
    const entt::entity station = registry.create();
    const entt::entity facility = MakeFacility(registry, MountId("lab"));

    KnowledgeNetwork network;
    network.unlockedBlueprints.insert("pulse_cannon_i");

    const auto rows = Candidates(registry, requester, facility, station, &network, 0, 0);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].alreadyKnown);
    CHECK(rows[0].row.style.disabled);
    CHECK(rows[0].row.value == "ALREADY KNOWN");
}

TEST_CASE("Candidates disables every row once the facility's queue is at capacity",
          "[research-screen]") {
    entt::registry registry;
    const entt::entity requester =
        MakeCargoRequester(registry, {ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 1.0f}});
    const entt::entity station = registry.create();
    const entt::entity facility = MakeFacility(registry, MountId("lab"), /*grade=*/1,
                                               /*capacity=*/1);

    const auto rows = Candidates(registry, requester, facility, station, nullptr,
                                 /*queuedCount=*/1, /*capacity=*/1);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].noSlots);
    CHECK(rows[0].row.value == "NO SLOTS");
}

TEST_CASE("Candidates disables a row already queued at this facility", "[research-screen]") {
    entt::registry registry;
    const entt::entity requester =
        MakeCargoRequester(registry, {ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 1.0f}});
    const entt::entity station = registry.create();
    const MountId lab("lab");
    const entt::entity facility = MakeFacility(registry, lab);

    StationFacility stationFacility;
    stationFacility.researchJobs.push_back(
        ResearchJob{ModuleId("pulse_cannon_i"), 0.0f, 60.0f, sr::KnowledgeNetworkId("net"), lab});
    registry.emplace<StationFacility>(station, stationFacility);

    const auto rows = Candidates(registry, requester, facility, station, nullptr, 1, 0);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].alreadyQueued);
    CHECK(rows[0].row.value == "QUEUED");
}

TEST_CASE("QueueRows lists only jobs whose facility matches this hardpoint's mount",
          "[research-screen]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const MountId thisLab("lab-a");
    const MountId otherLab("lab-b");
    const entt::entity facility = MakeFacility(registry, thisLab);

    StationFacility stationFacility;
    stationFacility.researchJobs.push_back(ResearchJob{ModuleId("pulse_cannon_i"), 3.0f, 10.0f,
                                                       sr::KnowledgeNetworkId("net"), thisLab});
    stationFacility.researchJobs.push_back(
        ResearchJob{ModuleId("shield_mk1"), 1.0f, 10.0f, sr::KnowledgeNetworkId("net"), otherLab});
    registry.emplace<StationFacility>(station, stationFacility);

    const auto rows = QueueRows(registry, station, facility);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].label == "pulse_cannon_i");
    CHECK(rows[0].fill == Catch::Approx(0.3f));
}

TEST_CASE("OwnedVesselAt finds the player's own docked vessel", "[research-screen]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, entt::null);
    registry.emplace<FactionRef>(vessel, FactionId("aegis"));

    CHECK(OwnedVesselAt(registry, station, FactionId("aegis")) == vessel);
}

TEST_CASE("OwnedVesselAt returns null with no owned vessel docked at the station",
          "[research-screen]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, entt::null);
    registry.emplace<FactionRef>(vessel, FactionId("kore"));

    CHECK((OwnedVesselAt(registry, station, FactionId("aegis")) == entt::null));
}

TEST_CASE("ActiveGaugeStatus resolves the current facility's own integrity", "[research-screen]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity facility = MakeFacility(registry, MountId("lab"));
    registry.emplace<ParentRig>(facility, station);
    registry.emplace<Health>(facility, 50.0f, 100.0f);
    registry.emplace<PlayerLocation>(facility, PlayerLocation{facility});
    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, entt::null);
    registry.emplace<FactionRef>(vessel, FactionId("aegis"));

    const auto status = ActiveGaugeStatus(registry, FactionId("aegis"));
    REQUIRE(status.has_value());
    CHECK(status->label == "RESEARCH LAB");
    CHECK(status->fraction == 0.5f);
}

TEST_CASE("ActiveGaugeStatus is nullopt while not viewing Research", "[research-screen]") {
    entt::registry registry;
    CHECK_FALSE(ActiveGaugeStatus(registry, FactionId("aegis")).has_value());
}
