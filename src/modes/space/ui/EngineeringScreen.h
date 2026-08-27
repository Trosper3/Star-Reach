#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <optional>
#include <string>
#include <vector>

#include "core/registries/ContentLibrary.h"
#include "modes/space/ui/BridgeView.h"
#include "shared/blueprints/Ids.h"
#include "shared/blueprints/RigBlueprint.h"
#include "shared/ui/Fonts.h"
#include "shared/ui/Row.h"

namespace sr::core {
class ContentLibrary;
}  // namespace sr::core

// modes/space/ui/EngineeringScreen -- architecture.md 12.30.5, "Screen 4 -- Engineering." Merges
// the old EngineerMenu (Merge/Deconstruct, CargoHold-facing) and RefactorMenu (Delete/Rebuild,
// rig-facing) behind their one shared gate: a living FacilityKind::Engineering hardpoint, active
// exactly while PlayerLocation names it -- the same pattern modes/space/ui/BayView.h's CurrentBay
// establishes. Only two of the four original verbs ship here: Delete and Rebuild are
// unconditionally correct; Merge is reachable-and-wrong until §12.21's Quality exists (unbounded,
// free, and producing ids that grow without bound), so it stays unrouted. Deconstruct has no UI
// trigger left at all -- see the fix note below; DeconstructModuleRequest and
// EngineerSystem::ProcessDeconstructRequests are unchanged and still correct, just uncalled.
//
// modes/*/ui/ must not include systems/ (section 2.3); this builds DeleteHardpointRequest and
// RebuildMountRequest (and, since the cross-hull drag-and-drop pass below, MountModuleRequest/
// UnmountModuleRequest and TransferItemRequest) for the caller to place on the requester -- the
// player's own vessel docked here, never PlayerControlled, which while docked is the station
// itself (the same named trap RepairScreen's own header comment documents) -- and never calls
// modes/space/systems/{EngineerSystem,RefactorSystem,ModuleEquipSystem,StationServicesSystem}
// directly.
//
// Issue #230's visual-chrome pass replaced the flat rig ListView with a schematic hardpoint-node
// view, using each MountBlueprint's already-authored `localOffset` (root-relative, world units --
// ShipBlueprint.h's own comment) rather than any invented layout. No chassis image sits behind
// the nodes: ShellDef::spriteLayer is the asset key reserved for exactly that (features.md 3.5's
// sprite-layer stack), but nothing in data/base_game/ authors a non-empty one today and no
// TextureCache dependency exists anywhere in modes/space/ -- so every rig in the game hits that
// "no graphic" fallback. A future spriteLayer-bearing Chassis shell is a small additive hook on
// top of this schematic, not a prerequisite for it.
//
// A follow-up pass reopened and reversed #230's "no drag-and-drop" call for THIS screen
// specifically (it stands unchanged for every other docked screen): the project owner wants real
// drag-and-drop here because Engineering is now the one screen that can show a vessel's and its
// docked station's CargoHold/rig at once, in any pairing -- a capability click-then-target cannot
// express (which of two panels does a click target?) and that ModulesMenu's own single-hull
// Loadout overlay cannot reach at all. The drag state (`EngineeringScreenState`,
// shared/components/Engineer.h) is screen-owned singleton state, the same "not widget-owned"
// distinction §12.30.7 already settled
// for FlightOverlayState -- ListView/Button gain nothing new.
//
// Two panels, each independently toggled between the requester's own hull and the docked
// station's (only ever offered when StationIsSubject -- a station is never a subject here unless
// it is player-owned, so Delete/Mount/Unmount never need a separate ownership check beyond
// RefactorSystem's/ModuleEquipSystem's own): CARGO HOLD (the left third) and SHIP RIG (the right
// two-thirds). Because the two selectors are independent, "vessel cargo + station rig" and
// "station cargo + vessel rig" are both reachable, which is what makes a cross-hull drag possible
// at all -- pick a module up from whichever cargo panel is showing, drop it on whichever rig panel
// is showing (Mount), or the reverse (an occupied hardpoint node dragged onto a cargo panel,
// Unmount). A straight cargo-to-cargo transfer (no mount involved) has no second cargo panel to
// drop on, so the two cargo-selector buttons double as drop targets themselves -- dropping a
// dragged module on "THIS STATION" while it is showing "YOUR VESSEL" sends it to the station's
// hold without switching what's on screen.
//
// Mount joins Delete/Rebuild as a verb this screen routes (built via MountModuleRequest/
// UnmountModuleRequest, shared/components/Equip.h, the same components ModuleEquipSystem already
// processes for the Loadout overlay -- their new sourceCargo/destinationCargo fields are exactly
// this cross-hull addition, entt::null preserving Loadout's own single-hull behaviour unchanged).
// Merge still does not ship here: §12.21's Quality per-instance still does not exist, so
// EngineerSystem::ProcessMergeRequests is reachable-and-correct on the system side but would still
// hand out the unbounded, ever-growing crafted-module id architecture.md already flags as a bug
// the moment a screen lets a player reach it -- and the project owner has agreed Merge belongs on
// its own facility once that lands, not folded into an already-busy Engineering screen.
//
// Fix, same pass: a plain click on a cargo row used to fire DeconstructModuleRequest (#230's
// original click-to-act) even after every row also became a drag source in the cross-hull pass --
// so a bare mis-click, no drag at all, silently destroyed a module the player never meant to touch.
// EndCargoDrag's unmoved-release case is now a pure cancel, the same "a mis-click refuses"
// convention ModulesMenu's own hold list already follows for its own click-vs-drag rows. Nothing
// in this screen calls DeconstructModuleRequest any more; a deliberate trigger (a dedicated
// button, say) would need to be added for Deconstruct to be reachable again.
//
// SHIP RIG also gained a live camera: a rig with several close-together mounts can pack their
// labels tightly enough to be unreadable, so the schematic is scroll-to-zoom (cursor-anchored --
// EngineeringScreen.cpp's own ZoomPanAdjust math keeps the node under the pointer fixed as the
// scale changes, rather than the whole view sliding) and right-drag-to-pan. Both live on
// EngineeringScreenState (rigZoom/rigPanOffset/rigPanning and its drag-start fields) and reset to
// defaults whenever the rig-subject toggle switches hulls, since a vessel's and a station's
// schematics share no frame of reference worth preserving across that switch. Node ring size and
// label/value text both scale with zoom -- the actual complaint was unreadable text, not just
// crowded rings, so only enlarging the rings would not have fixed it.
namespace sr::space::ui::engineering_screen {

// One row of the left list: one module stack in `requester`'s CargoHold. Pure -- no raylib --
// so unit-testable.
struct ModuleRow {
    ModuleId module;
    sr::ui::Row row;
};

// The one entity PlayerLocation currently names, or entt::null -- BayView.h's own CurrentBay
// establishes this pattern; duplicated here rather than shared since neither screen may depend on
// the other.
entt::entity PlayerShell(const entt::registry& registry);

// The living Engineering-kind facility hardpoint PlayerLocation currently names, or entt::null --
// the same "this screen is active exactly while standing on its own gate hardpoint" pattern
// modes/space/ui/BayView.h's CurrentBay establishes.
entt::entity CurrentFacility(const entt::registry& registry, entt::entity shell);

// Every living Engineering-kind hardpoint on `station`, in Rig::children order -- every sibling
// bench, unlike DockedFacility (shared/rig/DockedFacility.h) which resolves only the one
// PlayerLocation currently names. Mirrors BayView.h's own SiblingBays.
std::vector<entt::entity> SiblingBenches(const entt::registry& registry, entt::entity station);

// Every ItemKind::Module stack across `requester`'s cargo bays -- what the CargoHold panel lists,
// each row a drag source for Mount/Unmount/transfer (this file's own header comment on why a
// plain click no longer Deconstructs one).
std::vector<ModuleRow> ModuleRows(const entt::registry& registry, entt::entity requester);

// Every hardpoint on `rigRoot` no other hardpoint's StructuralAttachment points at -- deleting a
// non-leaf hardpoint would orphan its children, which RefactorSystem itself also refuses; this is
// what the screen uses to grey those out rather than let the player pick them and fail.
std::vector<entt::entity> DeletableHardpoints(const entt::registry& registry, entt::entity rigRoot);

// Every mount `blueprint` authors that has no corresponding entity anywhere in `rigRoot`'s
// Rig::children -- DeletableHardpoints's mirror (architecture.md 12.30.5). A mount with a
// Destroyed entity still present is NOT here: Delete must remove it first before the same mount
// id becomes a true gap.
std::vector<MountId> RebuildableMounts(const entt::registry& registry, entt::entity rigRoot,
                                       const RigBlueprint& blueprint);

// One row of the right list: one mount `blueprint` authors, joined against the live rig. Three
// states, all drawn (architecture.md 8.3: absence must never look like emptiness): `hardpoint` is
// entt::null for an authored-but-absent mount (the Rebuild verb); otherwise `destroyed` and
// `deletable` describe a living or Destroyed hardpoint (the Delete verb either way -- a Destroyed
// one simply returns nothing). `holdsModules` is this fix's own answer to the silent-refusal bug:
// RefactorSystem has always refused to delete a hardpoint that still holds a module (architecture
// .md 15.2 finding 8, "unmount first, then delete"), but this screen never surfaced why Delete did
// nothing -- `holdsModules` true disables the row with "UNMOUNT FIRST" regardless of `deletable`,
// and doubles as the query the drag state machine uses to decide whether picking up a node starts
// an Unmount drag at all.
struct MountRow {
    MountId mount;
    entt::entity hardpoint = entt::null;
    bool destroyed = false;
    bool deletable = false;
    bool holdsModules = false;
    sr::ui::Row row;
};

// `rigRoot`'s full mount list in `blueprint`'s authored order, joined against the live rig -- the
// one list in the game that can show a mount that is missing. Index-aligned with
// `blueprint.mounts` -- the schematic drawer zips this against `blueprint.mounts[i].localOffset`
// by position rather than carrying a copy of the offset on MountRow itself.
std::vector<MountRow> MountRows(const entt::registry& registry, entt::entity rigRoot,
                                const RigBlueprint& blueprint);

// The player's own vessel (FactionRef == playerFaction) currently Docked at `station`, or
// entt::null if none -- the same OwnedVesselAt shape BayView/RepairScreen already establish.
entt::entity OwnedVesselAt(const entt::registry& registry, entt::entity station,
                           const FactionId& playerFaction);

// True when `station`'s own rig is a second valid subject for this screen -- architecture.md
// 12.30.5's "Editing the station's own rig, when it is yours": its FactionRef matches
// `playerFaction`, the same ownership test §12.30.4's Repair screen already establishes for its
// own dual-subject section ("a station with a repair bay repairs itself"). Pure -- no content
// lookup, no raylib -- so unit-testable; the caller still needs the station's own BlueprintRef to
// resolve before the section can actually draw anything, which is not this function's concern.
bool StationIsSubject(const entt::registry& registry, entt::entity station,
                      const FactionId& playerFaction);

// "wing_port" -> "WING PORT" -- a MountId is already a stable, human-legible snake_case word
// (RelationSeeding.cpp's FactionId ids read the same way, per BridgeView.h's FactionDisplayName
// comment), so this is a plain substitution rather than a display-name registry. The schematic's
// own per-node label.
std::string FormattedMountLabel(const std::string& id);

// The display name of the module MountedModules::ids.back() names for `hardpoint` (ModulesMenu's
// own "the topmost id is the one shown" precedent), or empty for a Destroyed/missing/module-free
// hardpoint. The schematic's secondary line, so a node shows what is actually mounted there, not
// just its structural state.
std::string MountedModuleLabel(const entt::registry& registry, const core::ContentLibrary& content,
                               entt::entity hardpoint);

// True if `hardpoint` carries a non-empty MountedModules -- RefactorSystem's own "unmount first"
// precondition, mirrored here so a Delete click can be refused-and-explained instead of silently
// dropped (this file's own header comment), and so the drag state machine knows a mouse-down on
// this node should pick up an Unmount drag rather than attempt Delete/Rebuild.
bool HardpointHoldsModules(const entt::registry& registry, entt::entity hardpoint);

// Whether dropping `module` on `mount` (a hardpoint belonging to `targetRig`) would be accepted by
// ModuleEquipSystem: `mount` actually belongs to `targetRig`, is not Destroyed, is not already
// occupied, and its ShellRole accepts `module`'s ModuleKind. Deliberately unconcerned with which
// rig's CargoHold `module` currently sits in -- Engineering's cross-hull Mount is exactly a drag
// where the source and target rig differ, and ModuleEquipSystem's own sourceCargo field is what
// makes that legal. A local duplicate of ModulesMenuModel.cpp's own CanMount, per architecture.md
// 12.30's "each of this batch's files stays independently buildable" rule -- neither screen may
// depend on the other.
bool CanMountHere(const entt::registry& registry, const core::ContentLibrary& content,
                  entt::entity targetRig, entt::entity mount, const ModuleId& module);

// The mandatory per-screen facility-health readout (features.md 3.4) for the Engineering
// hardpoint PlayerLocation currently names, fed to the router's top-bar Gauge via SpaceFlight's
// orchestration (mirrors RepairScreen.h's/ResearchScreen.h's own ActiveGaugeStatus) rather than
// this screen drawing its own in-page gauge any more (issue #230's visual-chrome pass). nullopt
// unless the screen is active and an owned vessel is docked there.
std::optional<bridge_view::GaugeStatus> ActiveGaugeStatus(const entt::registry& registry,
                                                          const FactionId& playerFaction);

// Reads this frame's input and, while the player stands on a living Engineering hardpoint,
// hit-tests the sibling selector (when the station has more than one bench), the rig- and cargo-
// subject toggle buttons (only present at all once a station subject exists), and either begins a
// drag or fires an immediate click, depending what the mouse went down on:
//   - a rig node with nothing mounted: Delete/Rebuild fires immediately, exactly as issue #230's
//     click-to-act (DeleteHardpointRequest/RebuildMountRequest, placed on the requester with
//     `subject` naming which rig -- vessel or station -- it applies to);
//   - a rig node that still holds a module: begins an Unmount drag instead (MountRow::
//     holdsModules is this fix's own answer to the silent "click Delete, nothing happens" bug --
//     the node is disabled for Delete and explains why rather than pretending a click would work);
//   - a cargo row: always begins a potential drag now. Released back where it started (within a
//     few pixels) it is a plain click, not a drag, and cancels -- the module stays exactly where
//     it was; dragged onto a compatible rig node it Mounts (MountModuleRequest, same-hull or
//     cross-hull via its sourceCargo field); dragged onto the *other* cargo-subject's toggle
//     button it is a straight cargo-to-cargo TransferItemRequest.
// An Unmount drag released on a cargo panel or toggle button sends the module to that hull's
// CargoHold (UnmountModuleRequest, same-hull or cross-hull via its destinationCargo field).
// Because the cargo and rig toggles are independent, any vessel/station pairing is reachable,
// which is what makes a cross-hull drag possible at all (this file's own header comment). No-op
// with no owned vessel docked here. `content` resolves blueprints for MountRows and module
// compatibility for CanMountHere -- threaded in by the caller (SpaceFlight.cpp) the same way
// ResearchScreen threads its own KnowledgeStore, since modes/*/ui/ may not include systems/ but
// content resolution is not a systems/ concern.
//
// Independently of all of the above, every frame also runs the SHIP RIG canvas's own camera: a
// mouse-wheel tick over it zooms (cursor-anchored), and a right-button drag pans -- this file's own
// header comment on why both reset when the rig-subject toggle switches hulls.
void Update(entt::registry& registry, const FactionId& playerFaction,
            const core::ContentLibrary& content);

// Draws the Engineering screen full-screen (architecture.md 12.30's frame; bridge_view::Draw
// already drew the one bezel, top bar and icon rail around the whole window, so this does not
// draw a second one at that scale, nor its own integrity gauge any more -- ActiveGaugeStatus above
// feeds that to the router's top bar instead): a fixed "ENGINEERING BAY" header with a
// GRADE/CREDITS stat line and the rig-subject toggle beneath it (top-right, only once a station
// subject exists), a sibling selector when the station has more than one bench, a bracket-bordered
// CARGO HOLD panel (the first third of the content width; bordered icon-box rows; its own
// cargo-subject toggle in the title row) and a bracket-bordered SHIP RIG panel (the remaining two
// thirds; the schematic hardpoint-node view issue #230 adopted, now also highlighting a valid/
// invalid drop target while a cargo-sourced drag hovers it, and drawn through Update()'s own live
// zoom/pan camera -- node ring size and label/value text both scale with it, scissored to the
// canvas since panning or zooming out can push a node past its edges) -- and, while a drag is
// live, a ghost label following the cursor. `fonts` is shared/ui/Fonts.h's Orbitron/Exo2 pair,
// replacing raylib's built-in bitmap font.
void Draw(const entt::registry& registry, const FactionId& playerFaction,
          const core::ContentLibrary& content, const sr::ui::Fonts& fonts);

}  // namespace sr::space::ui::engineering_screen
