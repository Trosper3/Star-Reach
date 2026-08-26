#include <catch2/catch_test_macros.hpp>

#include "core/registries/ContentLibrary.h"
#include "modes/space/ui/ModulesMenu.h"
#include "shared/components/FlightOverlay.h"
#include "shared/components/Rig.h"

using sr::Destroyed;
using sr::FlightOverlayState;
using sr::FlightOverlayStateSingleton;
using sr::MountedModules;
using sr::ParentRig;
using sr::Rig;
using sr::ShellRole;
namespace modules_menu = sr::space::ui::modules_menu;

TEST_CASE("EquippableMounts lists only mounts whose MountedModules is empty or absent",
          "[modules-menu]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity empty = registry.create();
    registry.emplace<ShellRole>(empty, sr::ShellKind::Weapon);
    const entt::entity full = registry.create();
    registry.emplace<ShellRole>(full, sr::ShellKind::Weapon);
    registry.emplace<MountedModules>(full, MountedModules{{sr::ModuleId("pulse_cannon_i")}});
    registry.emplace<Rig>(root, std::vector<entt::entity>{empty, full});

    const auto mounts = modules_menu::EquippableMounts(registry, root);
    REQUIRE(mounts.size() == 1);
    CHECK(mounts.front() == empty);
}

TEST_CASE(
    "EquippableMounts treats a hardpoint with an empty (but present) MountedModules as available",
    "[modules-menu]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity emptyButPresent = registry.create();
    registry.emplace<ShellRole>(emptyButPresent, sr::ShellKind::Weapon);
    registry.emplace<MountedModules>(emptyButPresent);  // RigFactory always emplaces this.
    registry.emplace<Rig>(root, std::vector<entt::entity>{emptyButPresent});

    const auto mounts = modules_menu::EquippableMounts(registry, root);
    REQUIRE(mounts.size() == 1);
    CHECK(mounts.front() == emptyButPresent);
}

TEST_CASE("EquippedMounts lists only mounts with a non-empty MountedModules", "[modules-menu]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity empty = registry.create();
    registry.emplace<ShellRole>(empty, sr::ShellKind::Weapon);
    const entt::entity full = registry.create();
    registry.emplace<ShellRole>(full, sr::ShellKind::Weapon);
    registry.emplace<MountedModules>(full, MountedModules{{sr::ModuleId("pulse_cannon_i")}});
    registry.emplace<Rig>(root, std::vector<entt::entity>{empty, full});

    const auto mounts = modules_menu::EquippedMounts(registry, root);
    REQUIRE(mounts.size() == 1);
    CHECK(mounts.front() == full);
}

TEST_CASE(
    "EquippedMounts reads a blueprint-mounted hardpoint as occupied, not just a runtime-mounted "
    "one",
    "[modules-menu]") {
    // Regression test for architecture.md 13.3 finding C / 13.4 decision 2: before this,
    // EquippableMounts/EquippedMounts checked EquippedModule alone, which RigFactory never
    // writes -- every blueprint-mounted hardpoint on a fresh ship read as an empty slot. Mounting
    // there silently destroyed the original module; scrapping the hardpoint duplicated it.
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity factoryMount = registry.create();
    registry.emplace<ShellRole>(factoryMount, sr::ShellKind::Weapon);
    // RigFactory::CreateHardpoint's own write path -- never touches EquippedModule.
    registry.emplace<MountedModules>(factoryMount, MountedModules{{sr::ModuleId("gun_nose")}});
    registry.emplace<Rig>(root, std::vector<entt::entity>{factoryMount});

    CHECK(modules_menu::EquippableMounts(registry, root).empty());
    const auto equipped = modules_menu::EquippedMounts(registry, root);
    REQUIRE(equipped.size() == 1);
    CHECK(equipped.front() == factoryMount);
}

TEST_CASE("EquippableMounts/EquippedMounts are empty for a rig with no Rig component",
          "[modules-menu]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    CHECK(modules_menu::EquippableMounts(registry, root).empty());
    CHECK(modules_menu::EquippedMounts(registry, root).empty());
}

TEST_CASE(
    "EquippableMounts/EquippedMounts exclude a Destroyed mount even if it is empty or occupied",
    "[modules-menu]") {
    // architecture.md 12.30.7: the mount list's three states are occupied, empty, and Destroyed
    // -- a Destroyed hardpoint is never offered as an equip or unmount target regardless of what
    // MountedModules still says, so it must not appear in either list.
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity destroyedEmpty = registry.create();
    registry.emplace<ShellRole>(destroyedEmpty, sr::ShellKind::Weapon);
    registry.emplace<Destroyed>(destroyedEmpty);
    const entt::entity destroyedOccupied = registry.create();
    registry.emplace<ShellRole>(destroyedOccupied, sr::ShellKind::Weapon);
    registry.emplace<MountedModules>(destroyedOccupied, MountedModules{{sr::ModuleId("gun_nose")}});
    registry.emplace<Destroyed>(destroyedOccupied);
    registry.emplace<Rig>(root, std::vector<entt::entity>{destroyedEmpty, destroyedOccupied});

    CHECK(modules_menu::EquippableMounts(registry, root).empty());
    CHECK(modules_menu::EquippedMounts(registry, root).empty());
}

TEST_CASE("DestroyedMounts lists only Destroyed hardpoints, occupied or not", "[modules-menu]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity living = registry.create();
    registry.emplace<ShellRole>(living, sr::ShellKind::Weapon);
    const entt::entity destroyed = registry.create();
    registry.emplace<ShellRole>(destroyed, sr::ShellKind::Weapon);
    registry.emplace<Destroyed>(destroyed);
    registry.emplace<Rig>(root, std::vector<entt::entity>{living, destroyed});

    const auto mounts = modules_menu::DestroyedMounts(registry, root);
    REQUIRE(mounts.size() == 1);
    CHECK(mounts.front() == destroyed);
}

TEST_CASE("CanMount refuses a Destroyed mount", "[modules-menu]") {
    entt::registry registry;
    sr::core::ContentLibrary content;  // Bare -- FindModule always resolves nullptr here.
    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);
    registry.emplace<Destroyed>(mount);

    CHECK_FALSE(
        modules_menu::CanMount(registry, content, root, mount, sr::ModuleId("pulse_cannon_i")));
}

TEST_CASE("CanMount refuses an already-occupied mount", "[modules-menu]") {
    entt::registry registry;
    sr::core::ContentLibrary content;
    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);
    registry.emplace<MountedModules>(mount, MountedModules{{sr::ModuleId("gun_nose")}});

    CHECK_FALSE(
        modules_menu::CanMount(registry, content, root, mount, sr::ModuleId("pulse_cannon_i")));
}

TEST_CASE("CanMount refuses a mount belonging to another rig", "[modules-menu]") {
    entt::registry registry;
    sr::core::ContentLibrary content;
    const entt::entity otherRoot = registry.create();
    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, otherRoot);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);

    CHECK_FALSE(
        modules_menu::CanMount(registry, content, root, mount, sr::ModuleId("pulse_cannon_i")));
}

TEST_CASE("CanMount refuses a module id that does not resolve in content", "[modules-menu]") {
    entt::registry registry;
    sr::core::ContentLibrary content;  // Loads nothing -- FindModule always returns nullptr.
    const entt::entity root = registry.create();
    const entt::entity mount = registry.create();
    registry.emplace<ParentRig>(mount, root);
    registry.emplace<ShellRole>(mount, sr::ShellKind::Weapon);

    CHECK_FALSE(
        modules_menu::CanMount(registry, content, root, mount, sr::ModuleId("pulse_cannon_i")));
}

TEST_CASE("BuildMountRequest/BuildUnmountRequest carry their fields through", "[modules-menu]") {
    const entt::entity mount = entt::entity{7};
    const auto mountRequest =
        modules_menu::BuildMountRequest(sr::ModuleId("pulse_cannon_i"), mount);
    CHECK(mountRequest.module == sr::ModuleId("pulse_cannon_i"));
    CHECK(mountRequest.mount == mount);

    const auto unmountRequest = modules_menu::BuildUnmountRequest(mount);
    CHECK(unmountRequest.mount == mount);
}

TEST_CASE("ModulesMenu::IsOpen is false with no singleton entity at all", "[modules-menu]") {
    entt::registry registry;
    CHECK_FALSE(modules_menu::IsOpen(registry));
}

TEST_CASE(
    "ModulesMenu::IsOpen reads FlightOverlayState::loadoutOpen, independent of "
    "StorageMenu's own inventoryOpen flag",
    "[modules-menu]") {
    entt::registry registry;
    const entt::entity singleton = registry.create();
    registry.emplace<FlightOverlayStateSingleton>(singleton);
    registry.emplace<FlightOverlayState>(singleton,
                                         FlightOverlayState{false, true, {}, entt::null});

    CHECK(modules_menu::IsOpen(registry));
}
