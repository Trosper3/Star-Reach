#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/registries/ContentLibrary.h"
#include "modes/space/factories/RigFactory.h"
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
using sr::MountedModules;
using sr::MountId;
using sr::ParentRig;
using sr::PlayerLocation;
using sr::RebuildMountRequest;
using sr::Rig;
using sr::core::ContentLibrary;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace refactor_system = sr::space::refactor_system;
namespace rig_factory = sr::space::rig_factory;

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

// Also stands the requester in the bench it creates -- shared/rig/DockedFacility.h reads
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

rig_factory::SpawnParams VanguardAt(float x, float y) {
    rig_factory::SpawnParams params;
    params.blueprint = sr::BlueprintId("aegis_vanguard");
    params.position = {x, y};
    return params;
}

// Deletion refuses a hardpoint that still holds modules (RefactorSystemTests.cpp's own
// coverage); every aegis_vanguard mount but the chassis authors exactly one, so clearing it here
// is standing precondition, not the thing under test.
void ClearModules(entt::registry& registry, entt::entity hardpoint) {
    if (auto* mounted = registry.try_get<MountedModules>(hardpoint)) {
        mounted->ids.clear();
    }
}

}  // namespace

TEST_CASE("Rebuild restores a deleted hardpoint from the blueprint", "[refactor]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeEngineeringStation(registry);
    const auto spawned = rig_factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(spawned.ok());
    registry.emplace<Docked>(spawned.root, station, entt::null);

    const entt::entity wingPort =
        rig_factory::FindHardpoint(registry, spawned.root, MountId("wing_port"));
    REQUIRE(registry.valid(wingPort));
    ClearModules(registry, wingPort);
    registry.emplace<DeleteHardpointRequest>(spawned.root, DeleteHardpointRequest{wingPort});
    refactor_system::Tick(MakeContext(world, intents, content));
    REQUIRE(
        (rig_factory::FindHardpoint(registry, spawned.root, MountId("wing_port")) == entt::null));

    registry.emplace<RebuildMountRequest>(spawned.root, RebuildMountRequest{MountId("wing_port")});
    refactor_system::Tick(MakeContext(world, intents, content));

    const entt::entity rebuilt =
        rig_factory::FindHardpoint(registry, spawned.root, MountId("wing_port"));
    REQUIRE(registry.valid(rebuilt));
    CHECK_FALSE(registry.all_of<Destroyed>(rebuilt));
    CHECK(registry.get<ParentRig>(rebuilt).root == spawned.root);
    CHECK(registry.get<MountedModules>(rebuilt).ids.empty());
    CHECK_FALSE(registry.all_of<RebuildMountRequest>(spawned.root));
}

TEST_CASE("A rebuilt mount carries no modules, and delete-rebuild-delete does not grow the hold",
          "[refactor]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeEngineeringStation(registry);
    const auto spawned = rig_factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(spawned.ok());
    registry.emplace<Docked>(spawned.root, station, entt::null);

    const entt::entity wingPort =
        rig_factory::FindHardpoint(registry, spawned.root, MountId("wing_port"));
    ClearModules(registry, wingPort);
    registry.emplace<DeleteHardpointRequest>(spawned.root, DeleteHardpointRequest{wingPort});
    refactor_system::Tick(MakeContext(world, intents, content));

    registry.emplace<RebuildMountRequest>(spawned.root, RebuildMountRequest{MountId("wing_port")});
    refactor_system::Tick(MakeContext(world, intents, content));
    const entt::entity rebuilt =
        rig_factory::FindHardpoint(registry, spawned.root, MountId("wing_port"));
    REQUIRE(registry.valid(rebuilt));
    REQUIRE(registry.get<MountedModules>(rebuilt).ids.empty());

    registry.emplace<DeleteHardpointRequest>(spawned.root, DeleteHardpointRequest{rebuilt});
    refactor_system::Tick(MakeContext(world, intents, content));

    // If rebuild had wrongly re-attached the blueprint's authored "pulse_cannon_i" (the
    // duplication bug architecture.md 12.30.5 names), this second delete would refuse -- Delete's
    // own "still holds modules" check -- and the mount would still be present. It is gone, so
    // rebuild came back bare.
    CHECK((rig_factory::FindHardpoint(registry, spawned.root, MountId("wing_port")) == entt::null));
}

TEST_CASE("Rebuilding a mount the blueprint does not author is refused", "[refactor]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeEngineeringStation(registry);
    const auto spawned = rig_factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(spawned.ok());
    registry.emplace<Docked>(spawned.root, station, entt::null);
    const std::size_t before = registry.get<Rig>(spawned.root).children.size();

    registry.emplace<RebuildMountRequest>(spawned.root,
                                          RebuildMountRequest{MountId("no_such_mount")});
    refactor_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Rig>(spawned.root).children.size() == before);
}

TEST_CASE(
    "Rebuilding a mount whose attachedTo parent is absent is refused, and rebuilding the parent "
    "first then the child succeeds",
    "[refactor]") {
    // aegis_vanguard's cockpit is attachedTo hold, and hold is attachedTo core -- a two-level
    // chain deep enough to exercise "you cannot hang a wing off a hull that is not there."
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeEngineeringStation(registry);
    const auto spawned = rig_factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(spawned.ok());
    registry.emplace<Docked>(spawned.root, station, entt::null);

    const entt::entity cockpit =
        rig_factory::FindHardpoint(registry, spawned.root, MountId("cockpit"));
    ClearModules(registry, cockpit);
    registry.emplace<DeleteHardpointRequest>(spawned.root, DeleteHardpointRequest{cockpit});
    refactor_system::Tick(MakeContext(world, intents, content));
    REQUIRE((rig_factory::FindHardpoint(registry, spawned.root, MountId("cockpit")) == entt::null));

    const entt::entity hold = rig_factory::FindHardpoint(registry, spawned.root, MountId("hold"));
    ClearModules(registry,
                 hold);  // cockpit (its only dependent) is already gone -- hold is a leaf.
    registry.emplace<DeleteHardpointRequest>(spawned.root, DeleteHardpointRequest{hold});
    refactor_system::Tick(MakeContext(world, intents, content));
    REQUIRE((rig_factory::FindHardpoint(registry, spawned.root, MountId("hold")) == entt::null));

    // Rebuilding cockpit now, with hold still absent, must be refused.
    registry.emplace<RebuildMountRequest>(spawned.root, RebuildMountRequest{MountId("cockpit")});
    refactor_system::Tick(MakeContext(world, intents, content));
    CHECK((rig_factory::FindHardpoint(registry, spawned.root, MountId("cockpit")) == entt::null));

    // Rebuild the parent first...
    registry.emplace<RebuildMountRequest>(spawned.root, RebuildMountRequest{MountId("hold")});
    refactor_system::Tick(MakeContext(world, intents, content));
    REQUIRE((rig_factory::FindHardpoint(registry, spawned.root, MountId("hold")) != entt::null));

    // ...then the child succeeds.
    registry.emplace<RebuildMountRequest>(spawned.root, RebuildMountRequest{MountId("cockpit")});
    refactor_system::Tick(MakeContext(world, intents, content));
    CHECK((rig_factory::FindHardpoint(registry, spawned.root, MountId("cockpit")) != entt::null));
}

TEST_CASE("Rebuild is refused when the requester is not standing in a living Engineering facility",
          "[refactor]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const auto spawned = rig_factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(spawned.ok());
    // Not docked anywhere -- no PlayerLocation, no facility.

    registry.emplace<RebuildMountRequest>(spawned.root, RebuildMountRequest{MountId("wing_port")});
    refactor_system::Tick(MakeContext(world, intents, content));

    CHECK((rig_factory::FindHardpoint(registry, spawned.root, MountId("wing_port")) != entt::null));
    CHECK_FALSE(registry.all_of<RebuildMountRequest>(spawned.root));
}
