#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/registries/ContentLibrary.h"
#include "modes/space/systems/ModuleEquipSystem.h"
#include "modes/space/systems/WeaponSystem.h"
#include "shared/components/Combat.h"
#include "shared/components/Equip.h"
#include "shared/components/Loot.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

using sr::CargoHold;
using sr::Destroyed;
using sr::EquippedModule;
using sr::FireIntent;
using sr::MountModuleRequest;
using sr::MountTraverse;
using sr::ParentRig;
using sr::Projectile;
using sr::Rig;
using sr::ShellRole;
using sr::Target;
using sr::UnmountModuleRequest;
using sr::Vec2;
using sr::WorldTransform;
using sr::core::ContentLibrary;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace module_equip_system = sr::space::module_equip_system;
namespace weapon_system = sr::space::weapon_system;

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

}  // namespace

TEST_CASE("ModuleEquipSystem mounts a real cargo module onto a compatible empty mount",
          "[module-equip][integration]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

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
    CHECK(registry.get<CargoHold>(root).modules.empty());
    REQUIRE(registry.all_of<EquippedModule>(mount));
    CHECK(registry.get<EquippedModule>(mount).id == sr::ModuleId("pulse_cannon_i"));
    REQUIRE(registry.all_of<sr::Weapon>(mount));
    CHECK(registry.get<sr::Weapon>(mount).damage == 15.0f);
    REQUIRE(registry.all_of<sr::PowerLoad>(mount));
}

TEST_CASE(
    "A runtime-mounted weapon carries its mount's real authored traverse, not a zero-width arc",
    "[module-equip][integration]") {
    // Regression test for architecture.md 13.3 finding D: ModuleEquipSystem::ProcessMountRequests
    // used to call AttachModuleComponents with a hardcoded 0.0f, so a live-refitted weapon's
    // FiringArc could never satisfy AimAt's withinArc test and essentially never fired.
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);
    registry.emplace<MountTraverse>(mount, 0.3f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount});

    CargoHold cargo;
    cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));
    registry.emplace<CargoHold>(root, cargo);
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});

    module_equip_system::Tick(SystemContext{world, intents, content, 1.0f / 60.0f, 0});

    REQUIRE(registry.all_of<sr::FiringArc>(mount));
    CHECK(registry.get<sr::FiringArc>(mount).halfWidthRadians == 0.3f);
}

TEST_CASE("A runtime-mounted weapon fires within its authored arc and refuses outside it",
          "[module-equip][integration]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    const SystemContext ctx{world, intents, content, 1.0f / 60.0f, 0};

    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);
    registry.emplace<MountTraverse>(mount, 0.3f);  // ~17 degrees either side of dead ahead.
    registry.emplace<WorldTransform>(mount, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount});

    CargoHold cargo;
    cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));
    registry.emplace<CargoHold>(root, cargo);
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});
    module_equip_system::Tick(ctx);
    REQUIRE(registry.all_of<sr::Weapon>(mount));

    const entt::entity enemyRig = registry.create();
    const entt::entity enemyHardpoint = registry.create();
    registry.emplace<WorldTransform>(enemyHardpoint, Vec2{100.0f, 0.0f}, 0.0f);  // Dead ahead.
    registry.emplace<Target>(root, Target{enemyRig, enemyHardpoint});
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(ctx);

    CHECK(registry.storage<Projectile>().size() == 1);
}

TEST_CASE("A runtime-mounted weapon does not fire at a target outside its authored arc",
          "[module-equip][integration]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    const SystemContext ctx{world, intents, content, 1.0f / 60.0f, 0};

    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);
    registry.emplace<MountTraverse>(mount, 0.3f);
    registry.emplace<WorldTransform>(mount, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount});

    CargoHold cargo;
    cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));
    registry.emplace<CargoHold>(root, cargo);
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});
    module_equip_system::Tick(ctx);
    REQUIRE(registry.all_of<sr::Weapon>(mount));

    const entt::entity enemyRig = registry.create();
    const entt::entity enemyHardpoint = registry.create();
    // Straight "above" the mount -- a 90-degree bearing, well outside the 0.3-radian arc.
    registry.emplace<WorldTransform>(enemyHardpoint, Vec2{0.0f, 100.0f}, 0.0f);
    registry.emplace<Target>(root, Target{enemyRig, enemyHardpoint});
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(ctx);

    CHECK(registry.storage<Projectile>().size() == 0);
}

TEST_CASE("ModuleEquipSystem refuses to mount onto a Destroyed hardpoint", "[module-equip]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    ContentLibrary content;  // Bare -- the Destroyed check refuses before content resolution.

    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);
    registry.emplace<Destroyed>(mount);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount});

    CargoHold cargo;
    cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));
    registry.emplace<CargoHold>(root, cargo);
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});

    module_equip_system::Tick(SystemContext{world, intents, content, 1.0f / 60.0f, 0});

    CHECK_FALSE(registry.all_of<EquippedModule>(mount));
    CHECK(registry.get<CargoHold>(root).modules.size() == 1);
}

TEST_CASE("Mount then unmount round-trips a real module back into CargoHold, components intact",
          "[module-equip][integration]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

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
    REQUIRE(registry.all_of<EquippedModule>(mount));

    registry.emplace<UnmountModuleRequest>(root, UnmountModuleRequest{mount});
    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<EquippedModule>(mount));
    CHECK_FALSE(registry.any_of<sr::Weapon, sr::FiringArc, sr::PowerLoad>(mount));
    REQUIRE(registry.get<CargoHold>(root).modules.size() == 1);
    CHECK(registry.get<CargoHold>(root).modules.front() == sr::ModuleId("pulse_cannon_i"));
}

TEST_CASE("ModuleEquipSystem refuses a module whose kind does not match the mount's shell",
          "[module-equip][integration]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity root = registry.create();
    const entt::entity weaponMount = registry.create();
    registry.emplace<ParentRig>(weaponMount, root);
    registry.emplace<ShellRole>(weaponMount, sr::ShellKind::Weapon);
    registry.emplace<Rig>(root, std::vector<entt::entity>{weaponMount});

    CargoHold cargo;
    cargo.modules.push_back(sr::ModuleId("deflector_i"));  // a shield_generator module
    registry.emplace<CargoHold>(root, cargo);
    registry.emplace<MountModuleRequest>(
        root, MountModuleRequest{sr::ModuleId("deflector_i"), weaponMount});

    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<EquippedModule>(weaponMount));
    CHECK(registry.get<CargoHold>(root).modules.size() == 1);
}
