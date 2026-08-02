#pragma once

#include <raylib.h>
#include <string>
#include <vector>

#include "shared/components/Loot.h"

// modes/space/ui/StorageMenu -- architecture.md 12.11 (features.md, CargoHold's own doc comment
// names this menu as its future reader). modes/*/ui/ must not include systems/ (section 2.3);
// this menu only reads CargoHold, it never mutates it.
namespace sr::space::ui::storage_menu {

// One row of text per cargo entry -- modules first, then materials with their quantity. Pure --
// no raylib -- so unit-testable.
std::vector<std::string> Rows(const CargoHold& cargo);

void Draw(const Rectangle& bounds, const CargoHold& cargo);

}  // namespace sr::space::ui::storage_menu
