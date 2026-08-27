#pragma once

#include <entt/entity/entity.hpp>

#include "shared/blueprints/Ids.h"
#include "shared/math/Vec2.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// architecture.md 12.12's EngineerMenu: merge two owned modules of the same ModuleKind into one,
// at a level-scaled loss on the secondary module's contribution. Set by input/UI on the docked
// rig root; consumed and cleared by EngineerSystem the same tick, the same idiom as Docking.h's
// DockRequest. Both ids must be present in the requester's own CargoHold.
//
// `primary` is kept as the merge's base; `secondary` contributes a fraction of its own stats,
// scaled by the docked Engineering facility's FacilityRef::grade (1-5): contribution =
// secondaryStat * (level * 0.1) -- a level-1 engineer preserves 10% of the secondary module,
// level 5 preserves 50%. This is a placeholder scale, not a tuned value, the same category
// architecture.md 12.7's rate-roll weights are flagged as.
struct MergeModulesRequest {
    ModuleId primary;
    ModuleId secondary;
};

// architecture.md 12.30.5's Deconstruct: breaks down one owned module in the requester's own
// CargoHold, at the docked Engineering facility's grade-scaled recovery. Set by input/UI on the
// docked rig root; consumed and cleared by EngineerSystem the same tick, the same idiom as
// MergeModulesRequest above.
//
// §12.19's Recipe (what an item is "made of") does not exist yet, so the recovery this yields is
// a flat credits-scaled placeholder, the same honesty RefactorSystem's rebuild verb already
// applies to its own cost -- the gate and the consume-one-item rule are what this verb is for
// until Recipe lands and it can yield real materials instead.
struct DeconstructModuleRequest {
    ModuleId module;
};

// Tag: the one entity per registry carrying EngineeringScreenState -- System.h's "one legitimate
// cache" exception (Law 6), the same FlightOverlayStateSingleton/RepairScreenStateSingleton
// precedent. Created lazily the first time the Engineering screen needs it.
struct EngineeringScreenStateSingleton {};

// Cross-hull drag-and-drop: which hull each of the screen's two panels currently shows, plus the
// live drag -- screen-owned singleton state, not widget-owned (Widgets.h's "no retained tree" rule
// is about ListView/Button themselves, never about a screen holding a pending drag; §12.30.7's
// loadout overlay settled this same point for FlightOverlayState). A station subject only ever
// exists when it is player-owned (StationIsSubject), so both booleans below are meaningless -- and
// ignored -- whenever there is no second subject at all.
struct EngineeringScreenState {
    bool cargoShowsStation = false;  // false: the requester's own CargoHold. true: the station's.
    bool rigShowsStation = false;    // false: the requester's own rig. true: the station's.

    // Mutually exclusive, mirrors FlightOverlayState's own draggedModule/draggedFromMount split.
    // Both empty/null means no drag is live.
    ModuleId draggedModule;                         // Picked up from a cargo row.
    entt::entity draggedModuleSource = entt::null;  // Which rig's CargoHold it came from.
    entt::entity draggedFromMount = entt::null;     // Picked up from an occupied hardpoint node.
    entt::entity draggedFromMountRig = entt::null;  // Which rig that hardpoint belongs to.

    // Where the mouse went down when the drag above started -- Vec2, not raylib's Vector2, since
    // components live in shared/ (Law 8: sr_shared does not link raylib). This is what lets a
    // cargo row keep its #230 plain-click Deconstruct: a release within a few pixels of this point
    // is a click, not a drag, and resolves as the row's original action instead of a drop.
    Vec2 dragStartCursor;

    // The SHIP RIG panel's own live camera: a busy rig's nodes can sit close enough together that
    // their labels overlap, so the schematic is scroll-to-zoom (cursor-anchored, so the node you
    // are zooming toward is the one that stays under the pointer) and right-drag-to-pan on top of
    // its existing auto-fit layout. `rigZoom` multiplies the auto-fit scale; `rigPanOffset` is a
    // screen-pixel offset added after it. Reset to defaults whenever the rig-subject toggle
    // switches which hull is shown (EngineeringScreen.cpp's own BeginGesture) -- the two hulls'
    // schematics have no shared frame of reference worth preserving across that switch.
    float rigZoom = 1.0f;
    Vec2 rigPanOffset;

    // The right-drag gesture itself -- separate from the left-button drag fields above, since a
    // camera pan and a module pick-up are two different gestures that can never overlap (different
    // mouse buttons). `rigPanDragStart`/`rigPanDragStartOffset` are the cursor position and
    // `rigPanOffset` at the moment the drag began, so the pan tracks total displacement rather than
    // drifting from per-frame deltas.
    bool rigPanning = false;
    Vec2 rigPanDragStart;
    Vec2 rigPanDragStartOffset;
};

}  // namespace sr
