#pragma once

#include <raylib.h>
#include <string>

// modes/space/ui/InventoryGrid -- the shared slot-rendering widget architecture.md 12.10's
// promotion note calls for: StorageMenu and ModulesMenu both draw a grid of item rows, so it
// lands here rather than being forked between them (Law 11's tie-breaker -- both consumers exist
// in this same commit, so sharing now is not premature).
namespace sr::space::ui::inventory_grid {

// Draws one labeled row at `index` within `bounds`, fixed row height. Both StorageMenu (cargo/
// material rows) and ModulesMenu (hardpoint/equipped-module rows) call this identically.
void DrawSlotRow(const Rectangle& bounds, const std::string& label, int index);

}  // namespace sr::space::ui::inventory_grid
