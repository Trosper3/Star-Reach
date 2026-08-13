#pragma once

#include <string>

#include "shared/blueprints/Ids.h"

namespace sr {

// The authored definition of a raw element: mass per unit, the number CargoView needs to weigh
// a deposit against a bay's slotCapacity. Loaded from data/base_game/elements.json (Law 10) --
// the same "shell/module content, JSON-authored" model ShellDef and ModuleDef already follow.
//
// Deliberately minimal: nothing else about an element (its role in crafting, its market price)
// has a consumer yet, so nothing else is authored here (Law 11).
struct ElementDef {
    ElementId id;
    std::string displayName;
    float mass = 0.0f;
};

}  // namespace sr
