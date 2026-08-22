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
// `occupied` is `vessel == playerControlled` (architecture.md 12.30.1's derived tag) -- the one
// row, if any, that shows Launch instead of Board. Pure -- no raylib -- so unit-testable.
struct BayRosterEntry {
    entt::entity vessel = entt::null;
    sr::ui::Row row;
    bool owned = false;
    bool occupied = false;
};

// The roster for one bay: every rig whose Docked.bay == `bay`, in Docked-component discovery
// order. A vessel in a sibling bay of the same station is never included.
std::vector<BayRosterEntry> Roster(const entt::registry& registry, entt::entity bay,
                                   entt::entity playerControlled, const FactionId& playerFaction);

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

// Writes UndockRequest on `vessel` -- AvionicsMenu's existing DockingSystem-consumed path,
// unchanged.
void Launch(entt::registry& registry, entt::entity vessel);

// Reads this frame's input and, while PlayerLocation names a living Docking-kind facility
// hardpoint, hit-tests the roster (a click on a row performs that row's verb directly -- Board or
// Launch, whichever the row shows) and the sibling-bay selector. No-op otherwise.
void Update(entt::registry& registry, const FactionId& playerFaction);

// Draws the Bay screen: header (bay name, occupancy, this bay hardpoint's integrity), sibling
// selector, roster. No-op unless PlayerLocation currently names a living Docking-kind facility
// hardpoint.
void Draw(const entt::registry& registry, const FactionId& playerFaction);

}  // namespace sr::space::ui::bay_view
