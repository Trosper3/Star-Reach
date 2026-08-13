#include <catch2/catch_test_macros.hpp>

#include "modes/space/systems/ModuleEquipSystem.h"
#include "shared/components/Combat.h"
#include "shared/components/Equip.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"

using sr::CargoHold;
using sr::ItemKind;
using sr::ItemStack;
using sr::MountedModules;
using sr::MountModuleRequest;
using sr::ParentRig;
using sr::Rig;
using sr::ShellRole;
using sr::UnmountModuleRequest;
using sr::Weapon;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace module_equip_system = sr::space::module_equip_system;
namespace cargo_view = sr::cargo_view;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

// CargoHold lives per cargo-bay hardpoint now (architecture.md 12.23) -- every requester needs
// one of these under its Rig before it can hold anything to mount/receive back.
entt::entity MakeCargoBay(entt::registry& registry, entt::entity root) {
    const entt::entity bay = registry.create();
    registry.emplace<ParentRig>(bay, root);
    registry.emplace<CargoHold>(bay, std::vector<ItemStack>{}, 10, 1000.0f);
    return bay;
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
    const entt::entity bay = MakeCargoBay(registry, root);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount, bay});
    cargo_view::Deposit(registry, root, ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 0.0f});
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});

    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<MountModuleRequest>(root));
    CHECK(cargo_view::Merged(registry, root).size() == 1);
    CHECK(registry.get<MountedModules>(mount).ids.empty());
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
    const entt::entity bay = MakeCargoBay(registry, root);
    registry.emplace<Rig>(root, std::vector<entt::entity>{bay});
    cargo_view::Deposit(registry, root, ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 0.0f});
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});

    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK(cargo_view::Merged(registry, root).size() == 1);
    CHECK_FALSE(registry.all_of<MountedModules>(mount));
}

TEST_CASE("ModuleEquipSystem clears a mount request even when the requester has no cargo bay",
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

TEST_CASE(
    "Unmounting a module whose id no longer resolves in content is refused, not silently "
    "discarded",
    "[module-equip]") {
    // A bare ContentLibrary resolves nothing -- exercising the refusal path this file's own
    // philosophy calls for. The success path (module resolves, hands back to a cargo bay) is
    // exercised with real content in tests/integration/ModuleEquipSystemTests.cpp.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);
    registry.emplace<MountedModules>(mount, MountedModules{{sr::ModuleId("pulse_cannon_i")}});
    registry.emplace<Weapon>(mount, 15.0f);
    const entt::entity bay = MakeCargoBay(registry, root);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount, bay});

    registry.emplace<UnmountModuleRequest>(root, UnmountModuleRequest{mount});

    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<UnmountModuleRequest>(root));
    REQUIRE(registry.get<MountedModules>(mount).ids.size() == 1);
    CHECK(registry.all_of<Weapon>(mount));
    CHECK(cargo_view::Merged(registry, root).empty());
}

TEST_CASE("Unmounting an empty mount is a no-op", "[module-equip]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<MountedModules>(mount);  // Present, but empty -- nothing to hand back.
    const entt::entity bay = MakeCargoBay(registry, root);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount, bay});
    registry.emplace<UnmountModuleRequest>(root, UnmountModuleRequest{mount});

    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK(cargo_view::Merged(registry, root).empty());
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
    registry.emplace<MountedModules>(mount, MountedModules{{sr::ModuleId("already_here")}});

    const entt::entity bay = MakeCargoBay(registry, root);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount, bay});
    cargo_view::Deposit(registry, root, ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 0.0f});
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});

    module_equip_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.get<MountedModules>(mount).ids.size() == 1);
    CHECK(registry.get<MountedModules>(mount).ids.front() == sr::ModuleId("already_here"));
    CHECK(cargo_view::Merged(registry, root).size() == 1);
}

TEST_CASE(
    "ModuleEquipSystem refuses to mount onto a factory-built hardpoint that already carries a "
    "module",
    "[module-equip]") {
    // Regression test for architecture.md 13.3 finding C: before MountedModules became the sole
    // record, a blueprint-mounted hardpoint (RigFactory's own write path, never EquippedModule)
    // read as empty here -- mounting into it silently overwrote the original's live components
    // without returning them to cargo.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);
    // RigFactory::CreateHardpoint's own write, unconditional, even when it holds nothing --
    // here it holds the ship's original factory-authored weapon.
    registry.emplace<MountedModules>(mount, MountedModules{{sr::ModuleId("gun_nose")}});
    registry.emplace<Weapon>(mount, 12.0f);

    const entt::entity bay = MakeCargoBay(registry, root);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount, bay});
    cargo_view::Deposit(registry, root, ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 0.0f});
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});

    module_equip_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.get<MountedModules>(mount).ids.size() == 1);
    CHECK(registry.get<MountedModules>(mount).ids.front() == sr::ModuleId("gun_nose"));
    CHECK(registry.get<Weapon>(mount).damage == 12.0f);     // Untouched -- never overwritten.
    CHECK(cargo_view::Merged(registry, root).size() == 1);  // Still in cargo, not consumed.
}
