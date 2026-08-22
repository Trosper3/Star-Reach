#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <raylib.h>
#include <string>
#include <vector>

#include "shared/components/Loot.h"

// modes/space/ui/StorageMenu -- architecture.md 12.30.7's inventory overlay (features.md 3.10):
// your own CargoHold, everywhere -- in flight or docked, gated on nothing (features.md 2.7's
// "live refit is unrestricted" applies to the whole overlay set, not just the loadout half).
// modes/*/ui/ must not include systems/ (section 2.3), but shared/rig/CargoView.h is a lower
// layer than either -- Update/Draw call cargo_view::Merged themselves rather than have the
// caller do it, since both need it every frame the overlay is open. This menu builds
// JettisonRequest (shared/components/Loot.h) for the caller to place on `rigRoot` and never
// calls modes/space/systems/LootSystem directly.
namespace sr::space::ui::storage_menu {

// One row of text per cargo entry -- modules first, then elements with their quantity, matching
// the pre-P0-10 CargoHold's own display order regardless of which bay a stack lives in or the
// order cargo_view::Merged happens to return them in. Pure -- no raylib -- so unit-testable.
std::vector<std::string> Rows(const std::vector<ItemStack>& stacks);

// `stacks` reordered into Rows()'s exact iteration order (modules first, then elements) -- so a
// ListView row index hit-tested against Rows(stacks) resolves back to the ItemStack Jettison
// should act on. Pure -- no raylib -- so unit-testable.
std::vector<ItemStack> OrderedStacks(const std::vector<ItemStack>& stacks);

// True once the overlay's own toggle key has been pressed an odd number of times. Read by
// SpaceFlight to decide whether flight input should still reach the world underneath (features
// .md 3.4: neither overlay pauses, but a click inside one must not also fire weapons).
bool IsOpen(const entt::registry& registry);

// Toggles open/closed on its own key (KEY_I -- not yet in features.md 3.6's still-📋 input map;
// see this .cpp for why) and, while open, hit-tests the list -- a click jettisons that row's
// whole stack (no quantity picker: click-to-act, the same simplification every P4 screen in this
// batch makes rather than inventing retained selection state).
void Update(entt::registry& registry, entt::entity rigRoot);

// Draws the overlay -- centered, semi-transparent (features.md 3.10) -- when open; a no-op
// otherwise.
void Draw(const entt::registry& registry, entt::entity rigRoot);

}  // namespace sr::space::ui::storage_menu
