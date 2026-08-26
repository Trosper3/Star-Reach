#pragma once

#include <raylib.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <string>
#include <vector>

#include "core/registries/ContentLibrary.h"
#include "shared/components/Loot.h"
#include "shared/ui/Fonts.h"

// modes/space/ui/StorageMenu -- architecture.md 12.30.7's inventory overlay (features.md 3.10):
// your own CargoHold, everywhere -- in flight or docked, gated on nothing (features.md 2.7's
// "live refit is unrestricted" applies to the whole overlay set, not just the loadout half).
// modes/*/ui/ must not include systems/ (section 2.3), but shared/rig/CargoView.h is a lower
// layer than either -- Update/Draw call cargo_view::Merged themselves rather than have the
// caller do it, since both need it every frame the overlay is open. This menu builds
// JettisonRequest (shared/components/Loot.h) for the caller to place on `rigRoot` and never
// calls modes/space/systems/LootSystem directly.
//
// Issue #229's redesign: the list groups by ItemKind (Material family per architecture.md
// 12.30.3 once features.md 2.10's roster exists to group by -- today that is only ever
// "Elements" and "Modules"), and jettison is a footer verb with a quantity stepper acting on a
// clicked-to-select row, rather than a click on any row instantly jettisoning that row's whole
// stack.
namespace sr::space::ui::storage_menu {

// One row of the grouped list: either a section header (`isHeader`, `headerLabel` set) or one
// cargo stack (`stack` set). Pure -- no raylib -- so unit-testable.
struct GroupedEntry {
    bool isHeader = false;
    std::string headerLabel;
    ItemStack stack;
};

// `stacks` split into an "ELEMENTS" group and a "MODULES" group (architecture.md 12.30.3's
// ItemKind-then-Material-family order, Material family deferred -- see this file's own header
// comment), each preceded by its own header entry. A group with nothing in it gets no header at
// all: features.md 8.3's "absence must never look like emptiness" cuts both ways -- an always-
// empty "MATERIALS" heading would be noise, not information, until some ItemStack actually
// reports a Material family. Pure -- no raylib -- so unit-testable.
std::vector<GroupedEntry> GroupedRows(const std::vector<ItemStack>& stacks);

// True once the overlay's own toggle key has been pressed an odd number of times. Read by
// SpaceFlight to decide whether flight input should still reach the world underneath (features
// .md 3.4: neither overlay pauses, but a click inside one must not also fire weapons).
bool IsOpen(const entt::registry& registry);

// Toggles open/closed on its own key (KEY_I -- not yet in features.md 3.6's still-📋 input map;
// see this .cpp for why) and, while open: a click on a stack row selects it (highlighted, not
// acted on); a click on the footer's -/+ steps FlightOverlayState::jettisonQuantity, clamped to
// [1, that row's own quantity]; a click JETTISON emits a JettisonRequest for the selected stack
// at the chosen quantity and clears the selection. A click on a header row, or anywhere the
// selected stack no longer resolves (already jettisoned to zero elsewhere), does nothing.
void Update(entt::registry& registry, entt::entity rigRoot);

// Draws the overlay -- centered, semi-transparent (features.md 3.10) -- when open; a no-op
// otherwise. `content` resolves each row's display name and (for a Module row) its identity
// glyph; `fonts` is shared/ui/Fonts.h's Orbitron/Exo2 pair, matching the docked-screen and
// loadout-overlay visual-chrome passes (issues #224-228).
void Draw(const entt::registry& registry, entt::entity rigRoot, const core::ContentLibrary& content,
          const sr::ui::Fonts& fonts);

}  // namespace sr::space::ui::storage_menu
