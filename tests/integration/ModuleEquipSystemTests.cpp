#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/registries/ContentLibrary.h"
#include "modes/space/systems/ModuleEquipSystem.h"
#include "modes/space/systems/RefactorSystem.h"
#include "modes/space/systems/WeaponSystem.h"
#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Equip.h"
#include "shared/components/Facility.h"
#include "shared/components/Loot.h"
#include "shared/components/Power.h"
#include "shared/components/Refactor.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/rig/CargoView.h"

using sr::CargoHold;
using sr::DeleteHardpointRequest;
using sr::Destroyed;
using sr::Docked;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::FireIntent;
using sr::ItemKind;
using sr::ItemStack;
using sr::MountedModules;
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
namespace refactor_system = sr::space::refactor_system;
namespace weapon_system = sr::space::weapon_system;
namespace cargo_view = sr::cargo_view;

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

// CargoHold lives per cargo-bay hardpoint now (architecture.md 12.23). Adds a bay hardpoint to
// `root`'s Rig::children alongside whatever mounts the test already declared, then stocks it.
entt::entity AddCargoBay(entt::registry& registry, entt::entity root) {
    const entt::entity bay = registry.create();
    registry.emplace<ParentRig>(bay, root);
    registry.emplace<CargoHold>(bay, std::vector<ItemStack>{}, 10, 1000.0f);
    registry.get<Rig>(root).children.push_back(bay);
    return bay;
}

void StockModule(entt::registry& registry, entt::entity root, const ContentLibrary& content,
                 const sr::ModuleId& moduleId) {
    const sr::ModuleDef* module = content.FindModule(moduleId);
    const float mass = module != nullptr ? module->mass : 0.0f;
    cargo_view::Deposit(registry, root, ItemStack{ItemKind::Module, moduleId.str(), 1, mass});
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
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon,
                                std::vector<sr::ModuleKind>{sr::ModuleKind::Weapon});
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount});
    AddCargoBay(registry, root);
    StockModule(registry, root, content, sr::ModuleId("pulse_cannon_i"));
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});

    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<MountModuleRequest>(root));
    CHECK(cargo_view::Merged(registry, root).empty());
    REQUIRE(registry.get<MountedModules>(mount).ids.size() == 1);
    CHECK(registry.get<MountedModules>(mount).ids.front() == sr::ModuleId("pulse_cannon_i"));
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
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon,
                                std::vector<sr::ModuleKind>{sr::ModuleKind::Weapon});
    registry.emplace<MountTraverse>(mount, 0.3f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount});
    AddCargoBay(registry, root);
    StockModule(registry, root, content, sr::ModuleId("pulse_cannon_i"));
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
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon,
                                std::vector<sr::ModuleKind>{sr::ModuleKind::Weapon});
    registry.emplace<MountTraverse>(mount, 0.3f);  // ~17 degrees either side of dead ahead.
    registry.emplace<WorldTransform>(mount, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount});
    AddCargoBay(registry, root);
    StockModule(registry, root, content, sr::ModuleId("pulse_cannon_i"));
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
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon,
                                std::vector<sr::ModuleKind>{sr::ModuleKind::Weapon});
    registry.emplace<MountTraverse>(mount, 0.3f);
    registry.emplace<WorldTransform>(mount, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount});
    AddCargoBay(registry, root);
    StockModule(registry, root, content, sr::ModuleId("pulse_cannon_i"));
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
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon,
                                std::vector<sr::ModuleKind>{sr::ModuleKind::Weapon});
    registry.emplace<Destroyed>(mount);
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount});
    AddCargoBay(registry, root);
    cargo_view::Deposit(registry, root, ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 0.0f});
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});

    module_equip_system::Tick(SystemContext{world, intents, content, 1.0f / 60.0f, 0});

    // Destroyed refuses before MountedModules is even touched -- get_or_emplace never runs.
    CHECK_FALSE(registry.all_of<MountedModules>(mount));
    CHECK(cargo_view::Merged(registry, root).size() == 1);
}

TEST_CASE(
    "Mount then unmount round-trips a real module back into the requester's cargo bay, "
    "components intact",
    "[module-equip][integration]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon,
                                std::vector<sr::ModuleKind>{sr::ModuleKind::Weapon});
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount});
    AddCargoBay(registry, root);
    StockModule(registry, root, content, sr::ModuleId("pulse_cannon_i"));
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});
    module_equip_system::Tick(MakeContext(world, intents, content));
    REQUIRE(registry.get<MountedModules>(mount).ids.size() == 1);

    registry.emplace<UnmountModuleRequest>(root, UnmountModuleRequest{mount});
    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<MountedModules>(mount).ids.empty());
    CHECK_FALSE(registry.any_of<sr::Weapon, sr::FiringArc, sr::PowerLoad>(mount));
    const std::vector<ItemStack> cargo = cargo_view::Merged(registry, root);
    REQUIRE(cargo.size() == 1);
    CHECK(cargo.front().id == "pulse_cannon_i");
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
    registry.emplace<ShellRole>(weaponMount, sr::ShellKind::Weapon,
                                std::vector<sr::ModuleKind>{sr::ModuleKind::Weapon});
    registry.emplace<Rig>(root, std::vector<entt::entity>{weaponMount});
    AddCargoBay(registry, root);
    StockModule(registry, root, content, sr::ModuleId("deflector_i"));  // a shield_generator
    registry.emplace<MountModuleRequest>(
        root, MountModuleRequest{sr::ModuleId("deflector_i"), weaponMount});

    module_equip_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<MountedModules>(weaponMount).ids.empty());
    CHECK(cargo_view::Merged(registry, root).size() == 1);
}

TEST_CASE("A runtime-mounted module is never refunded twice across unmount and scrap",
          "[module-equip][integration]") {
    // Regression test for architecture.md 13.3 finding C's second consequence: with two
    // unreconciled records, scrapping a hardpoint refunded MountedModules (the factory original)
    // regardless of what ModuleEquipSystem's own unmount had already handed back through
    // EquippedModule -- the player could receive the same module twice. With MountedModules the
    // sole record (and RefactorSystem's finding-8 reversal from P0-05: refuse rather than
    // refund), the two operations cannot both pay out the same module.
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    const SystemContext ctx{world, intents, content, 1.0f / 60.0f, 0};

    const entt::entity engineeringHardpoint = registry.create();
    registry.emplace<FacilityRef>(engineeringHardpoint, FacilityKind::Engineering, 1);
    const entt::entity station = registry.create();
    registry.emplace<Rig>(station, std::vector<entt::entity>{engineeringHardpoint});

    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    const entt::entity otherMount = registry.create();  // Keeps `mount` from being "last."
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ParentRig>(otherMount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon,
                                std::vector<sr::ModuleKind>{sr::ModuleKind::Weapon});
    registry.emplace<Rig>(root, std::vector<entt::entity>{mount, otherMount});
    registry.emplace<Docked>(root, station, entt::null);
    AddCargoBay(registry, root);
    StockModule(registry, root, content, sr::ModuleId("pulse_cannon_i"));
    registry.emplace<MountModuleRequest>(root,
                                         MountModuleRequest{sr::ModuleId("pulse_cannon_i"), mount});
    module_equip_system::Tick(ctx);
    REQUIRE(cargo_view::Merged(registry, root).empty());

    // Scrapping while still mounted is refused outright (P0-05) -- not a refund at all.
    registry.emplace<DeleteHardpointRequest>(root, DeleteHardpointRequest{mount});
    refactor_system::Tick(ctx);
    REQUIRE(registry.valid(mount));
    CHECK(cargo_view::Merged(registry, root).empty());

    // Unmount first: the one and only refund.
    registry.emplace<UnmountModuleRequest>(root, UnmountModuleRequest{mount});
    module_equip_system::Tick(ctx);
    REQUIRE(cargo_view::Merged(registry, root).size() == 1);

    // Now scrapping succeeds -- MountedModules is empty, so there is nothing left to refund.
    registry.emplace<DeleteHardpointRequest>(root, DeleteHardpointRequest{mount});
    refactor_system::Tick(ctx);
    CHECK_FALSE(registry.valid(mount));
    CHECK(cargo_view::Merged(registry, root).size() == 1);  // Still exactly one copy.
}
