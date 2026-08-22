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

// architecture.md 12.30.7: true once the overlay's own toggle key has been pressed an odd
// number of times. Same FlightOverlayState singleton the inventory overlay shares
// (shared/components/FlightOverlay.h).
bool IsOpen(const entt::registry& registry);

// Toggles open/closed on its own key and, while open, drives the "select, then target" refit
// flow: a click on a held module selects it as FlightOverlayState::pendingModule; a click on an
// empty/Destroyed-excluded mount with a pending selection commits a MountModuleRequest and
// clears it; a click on an occupied mount commits an UnmountModuleRequest directly (one click --
// there is only one module there to remove). No drag-and-drop (architecture.md 12.30.7: a drag
// is retained state spanning frames, which the widget layer forbids).
void Update(entt::registry& registry, entt::entity rigRoot);

// Draws the overlay when open; a no-op otherwise.
void Draw(const entt::registry& registry, entt::entity rigRoot);

}  // namespace sr::space::ui::modules_menu
