#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <vector>

#include "shared/blueprints/Ids.h"
#include "shared/blueprints/RigBlueprint.h"
#include "shared/ui/Row.h"

namespace sr::core {
class ContentLibrary;
}  // namespace sr::core

// modes/space/ui/EngineeringScreen -- architecture.md 12.30.5, "Screen 4 -- Engineering." Merges
// the old EngineerMenu (Merge/Deconstruct, CargoHold-facing) and RefactorMenu (Delete/Rebuild,
// rig-facing) behind their one shared gate: a living FacilityKind::Engineering hardpoint, active
// exactly while PlayerLocation names it -- the same pattern modes/space/ui/BayView.h's CurrentBay
// establishes. Only two of the four verbs ship here: Delete and Rebuild are unconditionally
// correct; Merge is reachable-and-wrong until §12.21's Quality exists (unbounded, free, and
// producing ids that grow without bound), so it stays unrouted. Deconstruct ships with a
// placeholder yield -- see shared/components/Engineer.h's DeconstructModuleRequest comment.
//
// modes/*/ui/ must not include systems/ (section 2.3); this builds DeconstructModuleRequest,
// DeleteHardpointRequest and RebuildMountRequest for the caller to place on the requester -- the
// player's own vessel docked here, never PlayerControlled, which while docked is the station
// itself (the same named trap RepairScreen's own header comment documents) -- and never calls
// modes/space/systems/{EngineerSystem,RefactorSystem} directly.
namespace sr::space::ui::engineering_screen {

// One row of the left list: one module stack in `requester`'s CargoHold. Pure -- no raylib --
// so unit-testable.
struct ModuleRow {
    ModuleId module;
    sr::ui::Row row;
};

// Every ItemKind::Module stack across `requester`'s cargo bays -- the Deconstruct axis.
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
// one simply returns nothing).
struct MountRow {
    MountId mount;
    entt::entity hardpoint = entt::null;
    bool destroyed = false;
    bool deletable = false;
    sr::ui::Row row;
};

// `rigRoot`'s full mount list in `blueprint`'s authored order, joined against the live rig -- the
// one list in the game that can show a mount that is missing.
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

// Reads this frame's input and, while the player stands on a living Engineering hardpoint,
// hit-tests the sibling selector (when the station has more than one), the left list (a click
// issues DeconstructModuleRequest) and each rig-mount subject's list (a click issues
// DeleteHardpointRequest or RebuildMountRequest -- whichever that row's state means -- placed on
// the requester with `subject` naming which rig it applies to). No-op otherwise, or with no owned
// vessel docked here. `content` resolves the requester's own BlueprintRef for MountRows -- the
// one thing this screen needs core/ for, threaded in by the caller (SpaceFlight.cpp) the same way
// ResearchScreen threads its own KnowledgeStore, since modes/*/ui/ may not include systems/ but
// content resolution is not a systems/ concern.
void Update(entt::registry& registry, const FactionId& playerFaction,
            const core::ContentLibrary& content);

// Draws the Engineering screen: header (facility name, grade, this hardpoint's integrity,
// credits), sibling selector, left CargoHold list, one right-hand rig-mount section per subject
// (YOUR VESSEL, and STATION when it is yours).
void Draw(const entt::registry& registry, const FactionId& playerFaction,
          const core::ContentLibrary& content);

}  // namespace sr::space::ui::engineering_screen
