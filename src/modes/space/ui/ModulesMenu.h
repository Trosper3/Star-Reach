#pragma once

#include <raylib.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <vector>

#include "shared/components/Equip.h"

// modes/space/ui/ModulesMenu -- architecture.md 12.11: equip/unequip a CargoHold module onto
// `rigRoot`'s live hardpoints. modes/*/ui/ must not include systems/ (section 2.3), so this
// builds MountModuleRequest/UnmountModuleRequest (shared/components/Equip.h) for the caller to
// place on `rigRoot` -- the same DockRequest idiom AvionicsMenu already uses -- and never calls
// modes/space/systems/ModuleEquipSystem itself.
namespace sr::space::ui::modules_menu {

// Every hardpoint on `rigRoot` that can receive a drop: carries ShellRole, and its MountedModules
// (shared/components/Rig.h) -- the single record of a mount's contents, architecture.md 13.4
// decision 2 -- is empty or absent. Pure -- no raylib -- so unit-testable.
std::vector<entt::entity> EquippableMounts(const entt::registry& registry, entt::entity rigRoot);

// Every hardpoint on `rigRoot` whose MountedModules is non-empty -- what Draw lists with an
// unmount affordance.
std::vector<entt::entity> EquippedMounts(const entt::registry& registry, entt::entity rigRoot);

MountModuleRequest BuildMountRequest(const ModuleId& module, entt::entity mount);
UnmountModuleRequest BuildUnmountRequest(entt::entity mount);

void Draw(const Rectangle& bounds, const entt::registry& registry, entt::entity rigRoot);

}  // namespace sr::space::ui::modules_menu
