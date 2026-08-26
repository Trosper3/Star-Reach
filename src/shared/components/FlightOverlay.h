#pragma once

#include <entt/entity/entity.hpp>
#include <string>

#include "shared/blueprints/Ids.h"
#include "shared/components/Loot.h"

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
    // list, and mouse-wheel position has to persist across frames. Loadout-only fields already
    // coexist here with draggedModule/draggedFromMount above.
    float holdScroll = 0.0f;
    float mountScroll = 0.0f;

    // The inventory overlay's own vertical scroll -- holdScroll/mountScroll's counterpart above,
    // for StorageMenu's one list instead of loadout's two columns.
    float storageScroll = 0.0f;
    // The inventory overlay's own selection + jettison quantity (issue #229's grouping/quantity
    // pass). Identifies a row by (hardpoint, kind, id) rather than a row index -- the row list
    // this indexes into now has group headers mixed in, and shifts every time the one verb this
    // screen owns actually fires, so an index would go stale the instant either happens.
    // `selectedHardpoint == entt::null` means "nothing selected."
    entt::entity selectedHardpoint = entt::null;
    ItemKind selectedKind = ItemKind::Element;
    std::string selectedId;
    int jettisonQuantity = 1;
};

}  // namespace sr
