#include <catch2/catch_test_macros.hpp>

#include "modes/space/systems/RefactorSystem.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Loot.h"
#include "shared/components/Refactor.h"
#include "shared/components/Rig.h"

using sr::CargoHold;
using sr::DeleteHardpointRequest;
using sr::Docked;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::MountedModules;
using sr::ParentRig;
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

entt::entity MakeEngineeringStation(entt::registry& registry) {
    const entt::entity hardpoint = registry.create();
    registry.emplace<FacilityRef>(hardpoint, FacilityKind::Engineering, 1);
    const entt::entity station = registry.create();
    registry.emplace<Rig>(station, std::vector<entt::entity>{hardpoint});
    return station;
}

}  // namespace

TEST_CASE("Deleting a hardpoint returns its modules to CargoHold and removes it from the rig",
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

    const entt::entity root = registry.create();
    registry.emplace<ParentRig>(hardpoint, root);
    registry.emplace<Rig>(root, std::vector<entt::entity>{hardpoint});
    registry.emplace<Docked>(root, station, entt::null);
    registry.emplace<CargoHold>(root);
    registry.emplace<DeleteHardpointRequest>(root, DeleteHardpointRequest{hardpoint});

    refactor_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<DeleteHardpointRequest>(root));
    CHECK_FALSE(registry.valid(hardpoint));
    CHECK(registry.get<Rig>(root).children.empty());
    REQUIRE(registry.get<CargoHold>(root).modules.size() == 1);
    CHECK(registry.get<CargoHold>(root).modules.front() == sr::ModuleId("pulse_cannon_i"));
}

TEST_CASE("Deletion is refused when CargoHold has no room for the returned modules", "[refactor]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = MakeEngineeringStation(registry);
    const entt::entity hardpoint = registry.create();
    MountedModules mounted;
    mounted.ids.push_back(sr::ModuleId("pulse_cannon_i"));
    registry.emplace<MountedModules>(hardpoint, mounted);

    const entt::entity root = registry.create();
    registry.emplace<ParentRig>(hardpoint, root);
    registry.emplace<Rig>(root, std::vector<entt::entity>{hardpoint});
    registry.emplace<Docked>(root, station, entt::null);
    CargoHold cargo;
    cargo.capacity = 1;
    cargo.modules.push_back(sr::ModuleId("already_full"));
    registry.emplace<CargoHold>(root, cargo);
    registry.emplace<DeleteHardpointRequest>(root, DeleteHardpointRequest{hardpoint});

    refactor_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(hardpoint));
    CHECK(registry.get<Rig>(root).children.size() == 1);
    CHECK(registry.get<CargoHold>(root).modules.size() == 1);
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
    registry.emplace<CargoHold>(root);
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
    registry.emplace<CargoHold>(root);
    registry.emplace<DeleteHardpointRequest>(root, DeleteHardpointRequest{hardpoint});

    refactor_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(hardpoint));
}
