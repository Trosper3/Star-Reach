#include <catch2/catch_test_macros.hpp>

#include "modes/space/systems/ModuleEquipSystem.h"
#include "shared/components/Combat.h"
#include "shared/components/Equip.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"

using sr::CargoHold;
using sr::EquippedModule;
using sr::MountModuleRequest;
using sr::ParentRig;
using sr::Rig;
using sr::ShellRole;
using sr::UnmountModuleRequest;
using sr::Weapon;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace module_equip_system = sr::space::module_equip_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

}  // namespace

// A bare ContentLibrary resolves no module ids (it loads nothing) -- these unit tests exercise
// every refusal path, which only needs FindModule() to return nullptr consistently. The
// successful-mount/round-trip path needs a module id that actually resolves, which needs real
// content -- see tests/integration/ModuleEquipSystemTests.cpp.

TEST_CASE("ModuleEquipSystem refuses and clears a request for an unresolvable module",
          "[module-equip]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount});

    CargoHold cargo;
    cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));
    registry.emplace<CargoHold>(root, cargo);
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});

    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<MountModuleRequest>(root));
    CHECK(registry.get<CargoHold>(root).modules.size() == 1);
    CHECK_FALSE(registry.all_of<EquippedModule>(mount));
}

TEST_CASE("ModuleEquipSystem refuses to mount onto a mount belonging to another rig",
          "[module-equip]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity otherRoot = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, otherRoot);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);

    const entt::entity root = registry.create();
    CargoHold cargo;
    cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));
    registry.emplace<CargoHold>(root, cargo);
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});

    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<CargoHold>(root).modules.size() == 1);
    CHECK_FALSE(registry.all_of<EquippedModule>(mount));
}

TEST_CASE("ModuleEquipSystem clears a mount request even when the requester has no CargoHold",
          "[module-equip]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<MountModuleRequest>(
        root, MountModuleRequest{sr::ModuleId("pulse_cannon_i"), entt::entity{999}});

    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<MountModuleRequest>(root));
}

TEST_CASE("Unmounting a module hands its id back to CargoHold and clears EquippedModule",
          "[module-equip]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);
    registry.emplace<EquippedModule>(mount, sr::ModuleId("pulse_cannon_i"));
    registry.emplace<Weapon>(mount, 15.0f);
    registry.emplace<CargoHold>(root);

    registry.emplace<UnmountModuleRequest>(root, UnmountModuleRequest{mount});

    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<UnmountModuleRequest>(root));
    CHECK_FALSE(registry.all_of<EquippedModule>(mount));
    REQUIRE(registry.get<CargoHold>(root).modules.size() == 1);
    CHECK(registry.get<CargoHold>(root).modules.front() == sr::ModuleId("pulse_cannon_i"));
}

TEST_CASE("ModuleEquipSystem refuses to mount onto an already-equipped mount", "[module-equip]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);
    registry.emplace<EquippedModule>(mount, sr::ModuleId("already_here"));

    CargoHold cargo;
    cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));
    registry.emplace<CargoHold>(root, cargo);
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});

    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<EquippedModule>(mount).id == sr::ModuleId("already_here"));
    CHECK(registry.get<CargoHold>(root).modules.size() == 1);
}
