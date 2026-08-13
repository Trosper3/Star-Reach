#pragma once

#include <raylib.h>
#include <string>
#include <vector>

#include "shared/components/Loot.h"

// modes/space/ui/StorageMenu -- architecture.md 12.11 (features.md, CargoHold's own doc comment
// names this menu as its future reader). modes/*/ui/ must not include systems/ (section 2.3),
// but shared/rig/CargoView.h is a lower layer than either -- the caller merges the rig's living
// bays via cargo_view::Merged and hands the result in; this menu only reads it, it never mutates
// anything and never walks a Rig itself.
namespace sr::space::ui::storage_menu {

// One row of text per cargo entry -- modules first, then elements with their quantity, matching
// the pre-P0-10 CargoHold's own display order regardless of which bay a stack lives in or the
// order cargo_view::Merged happens to return them in. Pure -- no raylib -- so unit-testable.
std::vector<std::string> Rows(const std::vector<ItemStack>& stacks);

void Draw(const Rectangle& bounds, const std::vector<ItemStack>& stacks);

}  // namespace sr::space::ui::storage_menu
