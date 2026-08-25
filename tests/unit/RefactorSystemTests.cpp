#include <catch2/catch_test_macros.hpp>

#include "modes/space/systems/RefactorSystem.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Refactor.h"
#include "shared/components/Rig.h"

using sr::DeleteHardpointRequest;
using sr::Destroyed;
using sr::Docked;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::FactionId;
using sr::FactionRef;
using sr::MountedModules;
using sr::ParentRig;
using sr::PlayerLocation;
using sr::RebuildMountRequest;
using sr::Rig;
using sr::StructuralAttachment;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace refactor_system = sr::space::refactor_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

// Also stands the player in the bench it creates -- shared/rig/DockedFacility.h reads
// PlayerLocation, not a Rig::children scan.
entt::entity MakeEngineeringStation(entt::registry& registry) {
    const entt::entity hardpoint = registry.create();
    registry.emplace<FacilityRef>(hardpoint, FacilityKind::Engineering, 1);
    const entt::entity station = registry.create();
    registry.emplace<ParentRig>(hardpoint, station);
    registry.emplace<Rig>(station, std::vector<entt::entity>{hardpoint});
    registry.emplace<PlayerLocation>(hardpoint, PlayerLocation{hardpoint});
    return station;
}

}  // namespace

TEST_CASE(
    "Deletion is refused when the hardpoint still holds modules -- features.md 2.2's settled "
    "reversal",
    "[refactor]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = MakeEngineeringStation(registry);
    const entt::entity hardpoint = registry.create();
    MountedModules mounted;
    mounted.ids.push_back(sr::ModuleId("pulse_cannon_i"));
    registry.emplace<MountedModules>(hardpoint, mounted);
    const entt::entity otherHardpoint = registry.create();  // Keeps this from also being "last".

    const entt::entity root = registry.create();
    registry.emplace<ParentRig>(hardpoint, root);
    registry.emplace<ParentRig>(otherHardpoint, root);
    registry.emplace<Rig>(root, std::vector<entt::entity>{hardpoint, otherHardpoint});
    registry.emplace<Docked>(root, station, entt::null);
    registry.emplace<DeleteHardpointRequest>(root, DeleteHardpointRequest{hardpoint});

    refactor_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<DeleteHardpointRequest>(root));
    CHECK(registry.valid(hardpoint));
    CHECK(registry.get<Rig>(root).children.size() == 2);  // Refused, not refunded.
}

TEST_CASE("Deleting an empty hardpoint (no modules held) succeeds and removes it from the rig",
          "[refactor]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = MakeEngineeringStation(registry);
    const entt::entity hardpoint = registry.create();  // No MountedModules -- already emptied.
    const entt::entity otherHardpoint = registry.create();

    const entt::entity root = registry.create();
    registry.emplace<ParentRig>(hardpoint, root);
    registry.emplace<ParentRig>(otherHardpoint, root);
    registry.emplace<Rig>(root, std::vector<entt::entity>{hardpoint, otherHardpoint});
    registry.emplace<Docked>(root, station, entt::null);
    registry.emplace<DeleteHardpointRequest>(root, DeleteHardpointRequest{hardpoint});

    refactor_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<DeleteHardpointRequest>(root));
    CHECK_FALSE(registry.valid(hardpoint));
    CHECK(registry.get<Rig>(root).children.size() == 1);
}

TEST_CASE("Deletion of the last hardpoint is refused, and the rig survives", "[refactor]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = MakeEngineeringStation(registry);
    const entt::entity hardpoint = registry.create();  // No modules -- the only refusal reason

    const entt::entity root = registry.create();  // available here is "last hardpoint."
    registry.emplace<ParentRig>(hardpoint, root);
    registry.emplace<Rig>(root, std::vector<entt::entity>{hardpoint});
    registry.emplace<Docked>(root, station, entt::null);
    registry.emplace<DeleteHardpointRequest>(root, DeleteHardpointRequest{hardpoint});

    refactor_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(hardpoint));
    CHECK(registry.valid(root));
    CHECK(registry.get<Rig>(root).children.size() == 1);
}

TEST_CASE("Scrapping a Destroyed hardpoint refunds nothing, even if it still lists modules",
          "[refactor]") {
    // architecture.md 12.30.5: losing a hardpoint in combat costs the shell, not just nothing --
    // a Destroyed hardpoint is the one case that skips the "still holds modules" refusal above,
    // since scrapping it must proceed, but it must not hand those modules back either.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = MakeEngineeringStation(registry);
    const entt::entity hardpoint = registry.create();
    MountedModules mounted;
    mounted.ids.push_back(sr::ModuleId("pulse_cannon_i"));
    registry.emplace<MountedModules>(hardpoint, mounted);
    registry.emplace<Destroyed>(hardpoint);
    const entt::entity otherHardpoint = registry.create();

    const entt::entity root = registry.create();
    registry.emplace<ParentRig>(hardpoint, root);
    registry.emplace<ParentRig>(otherHardpoint, root);
    registry.emplace<Rig>(root, std::vector<entt::entity>{hardpoint, otherHardpoint});
    registry.emplace<Docked>(root, station, entt::null);
    registry.emplace<DeleteHardpointRequest>(root, DeleteHardpointRequest{hardpoint});

    refactor_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.valid(hardpoint));
    CHECK(registry.get<Rig>(root).children.size() == 1);
}

TEST_CASE("Deletion is refused for a hardpoint another hardpoint is structurally attached to",
          "[refactor]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = MakeEngineeringStation(registry);
    const entt::entity parentHardpoint = registry.create();
    const entt::entity childHardpoint = registry.create();
    registry.emplace<StructuralAttachment>(childHardpoint, parentHardpoint);

    const entt::entity root = registry.create();
    registry.emplace<ParentRig>(parentHardpoint, root);
    registry.emplace<ParentRig>(childHardpoint, root);
    registry.emplace<Rig>(root, std::vector<entt::entity>{parentHardpoint, childHardpoint});
    registry.emplace<Docked>(root, station, entt::null);
    registry.emplace<DeleteHardpointRequest>(root, DeleteHardpointRequest{parentHardpoint});

    refactor_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(parentHardpoint));
    CHECK(registry.get<Rig>(root).children.size() == 2);
}

TEST_CASE("Deletion is refused when the hardpoint does not belong to the requester's own rig",
          "[refactor]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = MakeEngineeringStation(registry);
    const entt::entity otherRoot = registry.create();
    const entt::entity hardpoint = registry.create();
    registry.emplace<ParentRig>(hardpoint, otherRoot);

    const entt::entity root = registry.create();
    registry.emplace<Docked>(root, station, entt::null);
    registry.emplace<DeleteHardpointRequest>(root, DeleteHardpointRequest{hardpoint});

    refactor_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(hardpoint));
}

TEST_CASE(
    "Delete acts on the station's own rig when subject names it and the station's FactionRef "
    "matches the requester's",
    "[refactor]") {
    // architecture.md 12.30.5's "Editing the station's own rig, when it is yours" -- `subject`
    // names which rig to edit; entt::null (every other test in this file) means "the requester's
    // own," matching the pre-P4-12 behavior unchanged.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = MakeEngineeringStation(registry);
    registry.emplace<FactionRef>(station, FactionId("aegis_directorate"));

    const entt::entity stationHardpoint = registry.create();
    const entt::entity stationOtherHardpoint = registry.create();  // Keeps it from being "last".
    registry.emplace<ParentRig>(stationHardpoint, station);
    registry.emplace<ParentRig>(stationOtherHardpoint, station);
    registry.get<Rig>(station).children.push_back(stationHardpoint);
    registry.get<Rig>(station).children.push_back(stationOtherHardpoint);

    const entt::entity root = registry.create();
    registry.emplace<FactionRef>(root, FactionId("aegis_directorate"));
    registry.emplace<Rig>(root, std::vector<entt::entity>{});
    registry.emplace<Docked>(root, station, entt::null);
    registry.emplace<DeleteHardpointRequest>(root,
                                             DeleteHardpointRequest{stationHardpoint, station});

    refactor_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.valid(stationHardpoint));
    // MakeEngineeringStation's own facility hardpoint plus stationOtherHardpoint remain.
    CHECK(registry.get<Rig>(station).children.size() == 2);
}

TEST_CASE(
    "Delete against the station's rig is refused when the station's FactionRef does not match "
    "the requester's",
    "[refactor]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = MakeEngineeringStation(registry);
    registry.emplace<FactionRef>(station, FactionId("scrappers"));

    const entt::entity stationHardpoint = registry.create();
    const entt::entity stationOtherHardpoint = registry.create();
    registry.emplace<ParentRig>(stationHardpoint, station);
    registry.emplace<ParentRig>(stationOtherHardpoint, station);
    registry.get<Rig>(station).children.push_back(stationHardpoint);
    registry.get<Rig>(station).children.push_back(stationOtherHardpoint);

    const entt::entity root = registry.create();
    registry.emplace<FactionRef>(root, FactionId("aegis_directorate"));
    registry.emplace<Rig>(root, std::vector<entt::entity>{});
    registry.emplace<Docked>(root, station, entt::null);
    registry.emplace<DeleteHardpointRequest>(root,
                                             DeleteHardpointRequest{stationHardpoint, station});

    refactor_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(stationHardpoint));
    CHECK(registry.get<Rig>(station).children.size() == 3);
}
