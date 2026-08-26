#pragma once

#include <entt/entity/entity.hpp>

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
    // architecture.md 12.30.7 (drag-and-drop, settled 2026-08-23 -- replaces this struct's
    // earlier click-then-target "pendingModule" field entirely, not just its meaning).
    // Mutually exclusive, whichever list the live drag started in: `draggedModule` holds a
    // CargoHold module id picked up from the hold list, `draggedFromMount` holds a hardpoint
    // entity picked up from an occupied mount in the mount list. Both empty/null means no drag
    // is live. Still screen-owned singleton state, not widget-owned -- ListView is only ever
    // asked "what row is under this point," the same question it already answered for a click.
    ModuleId draggedModule;
    entt::entity draggedFromMount = entt::null;
    // The loadout overlay's own per-column vertical scroll, in pixels -- a held CargoHold can
    // outgrow the fixed-height hold list the same way a many-hardpoint rig outgrows the mount
    // list, and mouse-wheel position has to persist across frames. Unused by the inventory
    // overlay (its own list scrolls the same way, tracked on StorageMenu's own state once that
    // gap is closed) -- loadout-only fields already coexist here with draggedModule/
    // draggedFromMount above.
    float holdScroll = 0.0f;
    float mountScroll = 0.0f;
};

}  // namespace sr
