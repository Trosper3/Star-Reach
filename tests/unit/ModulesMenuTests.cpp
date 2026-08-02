#include <catch2/catch_test_macros.hpp>

#include "modes/space/ui/ModulesMenu.h"
#include "shared/components/Rig.h"

using sr::EquippedModule;
using sr::Rig;
using sr::ShellRole;
namespace modules_menu = sr::space::ui::modules_menu;

TEST_CASE("EquippableMounts lists only mounts without an EquippedModule", "[modules-menu]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity empty = registry.create();
    registry.emplace<ShellRole>(empty, sr::ShellKind::Weapon);
    const entt::entity full = registry.create();
    registry.emplace<ShellRole>(full, sr::ShellKind::Weapon);
    registry.emplace<EquippedModule>(full, sr::ModuleId("pulse_cannon_i"));
    registry.emplace<Rig>(root, std::vector<entt::entity>{empty, full});

    const auto mounts = modules_menu::EquippableMounts(registry, root);
    REQUIRE(mounts.size() == 1);
    CHECK(mounts.front() == empty);
}

TEST_CASE("EquippedMounts lists only mounts with an EquippedModule", "[modules-menu]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    const entt::entity empty = registry.create();
    registry.emplace<ShellRole>(empty, sr::ShellKind::Weapon);
    const entt::entity full = registry.create();
    registry.emplace<ShellRole>(full, sr::ShellKind::Weapon);
    registry.emplace<EquippedModule>(full, sr::ModuleId("pulse_cannon_i"));
    registry.emplace<Rig>(root, std::vector<entt::entity>{empty, full});

    const auto mounts = modules_menu::EquippedMounts(registry, root);
    REQUIRE(mounts.size() == 1);
    CHECK(mounts.front() == full);
}

TEST_CASE("EquippableMounts/EquippedMounts are empty for a rig with no Rig component",
          "[modules-menu]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    CHECK(modules_menu::EquippableMounts(registry, root).empty());
    CHECK(modules_menu::EquippedMounts(registry, root).empty());
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
