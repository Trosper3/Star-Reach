#pragma once

#include <raylib.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <optional>
#include <vector>

#include "core/registries/ContentLibrary.h"
#include "shared/components/Equip.h"
#include "shared/components/FlightOverlay.h"
#include "shared/ui/Fonts.h"

// modes/space/ui/ModulesMenu -- architecture.md 12.30.7, "The loadout overlay": drag a held
// module from the hold list onto a compatible, unoccupied mount to equip it; drag an occupied
// mount back out onto the hold list to unmount it. modes/*/ui/ must not include systems/
// (section 2.3), so this builds MountModuleRequest/UnmountModuleRequest (shared/components/
// Equip.h) for the caller to place on `rigRoot` -- the same DockRequest idiom AvionicsMenu
// already uses -- and never calls modes/space/systems/ModuleEquipSystem itself.
namespace sr::space::ui::modules_menu {

// True if `mount`'s MountedModules (shared/components/Rig.h) -- the single record of a mount's
// contents, architecture.md 13.4 decision 2 -- is empty or absent. Pure -- no raylib -- so
// unit-testable. Declared here (rather than file-local) because both ModulesMenu.cpp's
// Update/Draw and ModulesMenuModel.cpp's Equippable/Equipped/CanMount read it.
bool IsEmpty(const entt::registry& registry, entt::entity mount);

// Every hardpoint on `rigRoot` that can receive a drop: carries ShellRole, is not Destroyed, and
// its MountedModules (shared/components/Rig.h) -- the single record of a mount's contents,
// architecture.md 13.4 decision 2 -- is empty or absent. Pure -- no raylib -- so unit-testable.
std::vector<entt::entity> EquippableMounts(const entt::registry& registry, entt::entity rigRoot);

// Every hardpoint on `rigRoot` that is not Destroyed and whose MountedModules is non-empty --
// what Draw lists with an unmount affordance.
std::vector<entt::entity> EquippedMounts(const entt::registry& registry, entt::entity rigRoot);

// Every hardpoint on `rigRoot` tagged Destroyed -- the loadout overlay's third row state
// (architecture.md 12.30.7: "living, empty, and Destroyed"). Shown disabled, never omitted
// (features.md 3.10's degrade-never-remove), and refuses as a drop target the same as it refused
// a click.
std::vector<entt::entity> DestroyedMounts(const entt::registry& registry, entt::entity rigRoot);

MountModuleRequest BuildMountRequest(const ModuleId& module, entt::entity mount);
UnmountModuleRequest BuildUnmountRequest(entt::entity mount);

// True if `module` could legally land on `mount` right now: `mount` belongs to `rigRoot`, is not
// Destroyed, is currently empty, and its ShellRole accepts `module`'s ModuleKind (architecture.md
// 12.22 -- content, not code). Pure -- no raylib -- so unit-testable. Mirrors
// ModuleEquipSystem::ProcessMountRequests's own refusal order without duplicating its ownership/
// possession checks, which only matter once a request is actually emitted -- "the UI is not
// authority" (architecture.md 12.30.7's test list), this is only the drag's own greying/hover
// rule, not a second source of truth for whether a mount finally accepts a module.
bool CanMount(const entt::registry& registry, const core::ContentLibrary& content,
              entt::entity rigRoot, entt::entity mount, const ModuleId& module);

// architecture.md 12.30.7: true once the overlay's own toggle key has been pressed an odd
// number of times. Same FlightOverlayState singleton the inventory overlay shares
// (shared/components/FlightOverlay.h).
bool IsOpen(const entt::registry& registry);

// Distinct Module ids held anywhere in `rigRoot`'s cargo, filtered to modules compatible with at
// least one hardpoint shape on this rig -- the loadout overlay's left `ListView` (architecture.md
// 12.30.7's layout table: "your hold, filtered to modules mountable somewhere on this rig"). Pure
// -- no raylib -- so unit-testable.
std::vector<ModuleId> HeldModules(const entt::registry& registry, entt::entity rigRoot,
                                  const core::ContentLibrary& content);

// Occupied, then empty, then destroyed -- the loadout overlay's right `ListView`, in the same
// order Update/Draw hit-test and render them. Pure -- no raylib -- so unit-testable.
std::vector<entt::entity> OrderedMounts(const entt::registry& registry, entt::entity rigRoot);

// `rigRoot`'s live mass/power totals -- the header's "before" numbers (architecture.md 12.30.7's
// layout table). Pure -- no raylib -- so unit-testable.
struct RigTotals {
    float mass = 0.0f;
    float powerGeneration = 0.0f;
    float powerDraw = 0.0f;
};
RigTotals CurrentTotals(const entt::registry& registry, entt::entity rigRoot);

// architecture.md 12.30.7: "the header numbers change while the drag is live and hovering a
// valid target, before the drop." `RecomputeRigTotals` is the real system's function and cannot
// run against a hypothetical without mutating the registry, so this is its read-only shadow --
// CurrentTotals plus or minus exactly the one module the live drag would add or remove, if
// `hoveredMount` names a mount CanMount currently accepts (for a held-module drag) or
// `hoveredHoldList` is true (for a mount drag). nullopt when nothing is hovering a committable
// target, in which case Draw falls back to CurrentTotals. Pure -- no raylib -- so unit-testable.
std::optional<RigTotals> PendingTotals(const entt::registry& registry,
                                       const core::ContentLibrary& content, entt::entity rigRoot,
                                       const FlightOverlayState& state,
                                       std::optional<entt::entity> hoveredMount,
                                       bool hoveredHoldList);

// Toggles open/closed on its own key and, while open, drives the drag-and-drop refit gesture
// (architecture.md 12.30.7, settled 2026-08-23 -- replaces the earlier click-then-target flow
// entirely): mouse down on a held module or an occupied mount picks it up as
// FlightOverlayState::draggedModule/draggedFromMount; mouse up over a compatible, unoccupied
// mount (for a held module) or over the hold list (for a mount) commits a
// MountModuleRequest/UnmountModuleRequest and clears the drag. Any other release -- an
// incompatible or occupied mount, empty space, back where it started -- cancels with no request
// emitted. `content` resolves CanMount's ShellRole/ModuleKind check.
void Update(entt::registry& registry, entt::entity rigRoot, const core::ContentLibrary& content);

// Draws the overlay when open; a no-op otherwise. `content` resolves each row's module name/mass/
// power and the header's live mass-and-power preview against the current drag's hover target;
// `fonts` is shared/ui/Fonts.h's Orbitron/Exo2 pair, matching the docked-screen visual-chrome
// pass (issues #224-227). While a drag is live, also draws a ghost -- a small row-styled label
// following the cursor, since the module being carried has to stay visible mid-drag
// (architecture.md 12.30.7).
void Draw(const entt::registry& registry, entt::entity rigRoot, const core::ContentLibrary& content,
          const sr::ui::Fonts& fonts);

}  // namespace sr::space::ui::modules_menu
