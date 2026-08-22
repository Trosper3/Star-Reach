#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <vector>

#include "core/knowledge/KnowledgeNetwork.h"
#include "shared/blueprints/Ids.h"
#include "shared/ui/Row.h"

// modes/space/ui/ResearchScreen -- architecture.md 12.30.6, "Screen 6 -- Research." Gated on a
// living FacilityKind::Research hardpoint, the same BayView/RepairScreen "active exactly while
// PlayerLocation names this hardpoint" pattern.
//
// This is the missing entry point for a mechanism that is otherwise fully built: ResearchSystem
// already advances, demotes, promotes and grants -- nothing anywhere produces the first
// ResearchJob. This screen builds StartResearchRequest (shared/components/Research.h) for the
// caller to place on the requester -- the docked vessel entity `OwnedVesselAt` resolves, never
// PlayerControlled, which while standing on this hardpoint IS the hardpoint (RepairScreen.h's
// documented trap) -- and never calls modes/space/systems/ResearchSystem directly
// (modes/*/ui/ may not include systems/, architecture.md section 2.3).
//
// Deliberately out of scope (architecture.md's fuller spec flags both as separate, unbuilt
// pieces): the sample survival roll and CargoHold consumption/return of the researched item, and
// a multi-lab sibling selector (architecture.md 12.30.5's TabStrip, needed once a station can
// carry more than one Research hardpoint reachable from the router -- today's BridgeView only
// ever routes to the first). The candidate list below reads CargoHold for "what do you actually
// have to research" without withdrawing anything -- research here costs time and a slot, not the
// item itself.
namespace sr::space::ui::research_screen {

// One candidate row: a distinct ModuleId held somewhere in the requester's CargoHold. Pure -- no
// raylib -- so unit-testable.
struct CandidateRow {
    ModuleId item;
    bool alreadyKnown = false;
    bool noSlots = false;
    bool alreadyQueued = false;
    sr::ui::Row row;
};

// Distinct Module-kind item ids held in `requester`'s CargoHold, each priced against `facility`'s
// grade-derived duration (research_system::DurationSeconds) and checked against `network`'s
// unlocked set, the station's current queue occupancy vs. `capacity` (0 unlimited), and whether
// that item is already queued at this facility. `RowStyle::disabled` is set whenever any refusal
// applies; the row is still drawn (features.md 3.10 degrade-never-remove), never hidden.
std::vector<CandidateRow> Candidates(const entt::registry& registry, entt::entity requester,
                                     entt::entity facility, entt::entity station,
                                     const core::knowledge::KnowledgeNetwork* network,
                                     int queuedCount, int capacity);

// One running-job row for `facility`'s own queue (StationFacility::researchJobs filtered to jobs
// whose MountId names this hardpoint) -- `Row::fill` carries progress/durationSeconds so
// DrawListView draws the per-row progress bar architecture.md 12.30.6 asks for.
std::vector<sr::ui::Row> QueueRows(const entt::registry& registry, entt::entity station,
                                   entt::entity facility);

// The player's own vessel (FactionRef == playerFaction) currently Docked at `station`, or
// entt::null if none. Duplicated locally per architecture.md 12.30's established rule (each
// docked-screen issue branches off main independently) rather than shared with the other
// screens' own copies.
entt::entity OwnedVesselAt(const entt::registry& registry, entt::entity station,
                          const FactionId& playerFaction);

// Reads this frame's input and, while the player stands on a living Research hardpoint,
// hit-tests the candidate list -- a click on an enabled row places a StartResearchRequest naming
// that item and this hardpoint's MountId on the requester. No-op on a disabled row.
void Update(entt::registry& registry, const FactionId& playerFaction,
           const core::knowledge::KnowledgeStore& knowledge);

// Draws the Research screen: header (lab name, grade, integrity, slots), the candidate ListView,
// and the running-job queue ListView.
void Draw(const entt::registry& registry, const FactionId& playerFaction,
         const core::knowledge::KnowledgeStore& knowledge);

}  // namespace sr::space::ui::research_screen
