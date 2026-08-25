#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <vector>

#include "shared/blueprints/Ids.h"
#include "shared/ui/Row.h"

// modes/space/ui/BayView -- architecture.md 12.30.2, "Screen 1 -- the Bay." The default
// PlayerLocation screen: DockingSystem now moves PlayerLocation onto a station's Docked.bay
// hardpoint the moment the player's own rig docks (modes/space/systems/DockingSystem.cpp), which
// is what makes this the screen a player lands on without clicking a router tab first.
//
// modes/*/ui/ must not include systems/ (section 2.3) -- the player's own FactionId
// (modes/space/systems/PlayerRecordSystem.h) is threaded in by the caller (SpaceFlight.cpp)
// rather than looked up here, the same reason AvionicsMenu takes its DiplomacyMatrix-shaped
// dependencies as parameters rather than reaching for systems/ itself. Per Law 9, Update() writes
// PlayerLocation and UndockRequest directly -- the same intent-emission idiom BridgeView::SelectTab
// and AvionicsMenu::Update already establish for this exact pair of writes.
namespace sr::space::ui::bay_view {

// One roster row: a vessel currently Docked at `bay` (architecture.md 12.30.2's "view<Docked>
// filtered on docked.bay == thisBay", no new component). `owned` is FactionRef == playerFaction;
// `occupied` is `vessel == occupiedVessel` -- the one row, if any, that shows Launch instead of
// Board. Pure -- no raylib -- so unit-testable.
struct BayRosterEntry {
    entt::entity vessel = entt::null;
    sr::ui::Row row;
    bool owned = false;
    bool occupied = false;
};

// The roster for one bay: every rig whose Docked.bay == `bay`, in Docked-component discovery
// order. A vessel in a sibling bay of the same station is never included. `occupiedVessel` is the
// vessel the player currently occupies at this station (OwnedVesselAt below) -- NOT
// registry.view<PlayerControlled>(), which while PlayerLocation names any facility hardpoint
// (this bay included) resolves to the station itself, never the docked vessel (architecture.md
// 12.30.1); passing that in here means the roster's own row never shows Launch.
std::vector<BayRosterEntry> Roster(const entt::registry& registry, entt::entity bay,
                                   entt::entity occupiedVessel, const FactionId& playerFaction);

// Every living FacilityKind::Docking hardpoint on `station`, in Rig::children order -- every
// sibling bay, unlike bridge_view::AvailableTabs which collapses to the first one. A one-element
// result is the common case (most stations author a single docking-bay module); the caller draws
// the sibling-selector strip only when this has more than one entry -- architecture.md 12.30.2's
// "absent when there is one bay."
std::vector<entt::entity> SiblingBays(const entt::registry& registry, entt::entity station);

// The player's own vessel (FactionRef == playerFaction) currently Docked at `station`, or
// entt::null if none. At most one exists today -- parking a second hull deletes it on warp
// (architecture.md 12.30.2's parked-hull gap, blocked on RigState/P10-01) -- so this never needs
// to disambiguate between two simultaneously-docked owned hulls yet. Shared by the roster's
// owned/occupied split and AvionicsMenu's "R = board and launch" shortcut.
entt::entity OwnedVesselAt(const entt::registry& registry, entt::entity station,
                           const FactionId& playerFaction);

// Moves PlayerLocation onto `vessel`'s cockpit (self-referential -- SpaceFlight::SpawnPlayerAt's
// own comment notes there is no separate cockpit hardpoint entity yet). One write; the derived
// PlayerControlled follows next tick (modes/space/systems/PlayerLocationSystem.h).
void Board(entt::registry& registry, entt::entity vessel);

// Boards `vessel` (Board above) and writes UndockRequest on it -- the click-path equivalent of
// AvionicsMenu's "R = board and launch" shortcut. Boarding first matters: while the player is
// standing on the bay hardpoint rather than aboard `vessel`, PlayerLocation would otherwise be
// left on the hardpoint after DockingSystem clears Docked (#207's meso-loop trace).
void Launch(entt::registry& registry, entt::entity vessel);

// Reads this frame's input and, while PlayerLocation names a living Docking-kind facility
// hardpoint, hit-tests the roster (a click on a row performs that row's verb directly -- Board or
// Launch, whichever the row shows) and the sibling-bay selector. No-op otherwise.
void Update(entt::registry& registry, const FactionId& playerFaction);

// Draws the Bay screen full-screen (architecture.md 12.30's frame; bridge_view::Draw already drew
// the one bezel around the whole window, so this does not draw a second one at that scale): header
// (bay name, one stat line combining slots/occupancy/integrity -- features.md 3.4's mandatory
// per-screen health readout), sibling selector, and the roster inside its own bracket-bordered
// sub-panel (issue #224's visual-chrome pass). No-op unless PlayerLocation currently names a living
// Docking-kind facility hardpoint and Storage is not the selected tab
// (bridge_view::IsStorageSelected).
void Draw(const entt::registry& registry, const FactionId& playerFaction);

}  // namespace sr::space::ui::bay_view
