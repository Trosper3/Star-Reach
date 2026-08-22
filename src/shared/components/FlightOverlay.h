#pragma once

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// Tag: the one entity per registry carrying FlightOverlayState -- System.h's "one legitimate
// cache" exception (Law 6), the same SystemMenuState/CommsLog precedent. Created lazily the
// first time either overlay's key is pressed, not up front.
struct FlightOverlayStateSingleton {};

// architecture.md 12.30.7: the inventory and loadout overlays share one open/closed-and-pending
// state, the same way SystemMenuState holds everything its one screen needs. Both are legal
// everywhere -- in flight or docked, over the world or over a docked screen (features.md 3.10) --
// and neither pauses (features.md 3.4).
struct FlightOverlayState {
    bool inventoryOpen = false;
    bool loadoutOpen = false;
    // The loadout overlay's "select, then target" flow: a CargoHold module id chosen from the
    // left ListView, waiting for a mount click to commit a MountModuleRequest. Empty means
    // nothing pending. Two stateless clicks rather than drag-and-drop (architecture.md 12.30.7:
    // "no retained tree, no widget-owned state" -- a drag is retained state spanning frames,
    // this is one more field on the same UI-state singleton a selected row index already is).
    ModuleId pendingModule;
};

}  // namespace sr
