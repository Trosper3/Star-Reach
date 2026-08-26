#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <optional>
#include <vector>

#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/ui/BridgeView.h"
#include "shared/blueprints/Ids.h"
#include "shared/ui/Fonts.h"
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
//
// The header's CODEX button (architecture.md 12.30.6's own Codex subsection) opens
// modes/space/ui/CodexScreen.h -- a separate, ungated read-only browse of everything the
// player's NetworkOwner already unlocks. This header owns hit-testing that one button; the panel
// it opens is entirely CodexScreen's own file, the same one-button coupling direction
// BridgeView's tabs have with the screens they route to.
//
// issue #227's visual-chrome pass: a fixed "RESEARCH LAB" header (the router's top bar already
// names the specific station under "DOCKED AT", the same call Repair's/Storage's own headers
// already made) with a consolidated GRADE/SLOTS stat line, over two bracket-bordered panels
// (candidates, then the running queue) drawn with Bay's/Storage's/Repair's own bordered icon-box
// row treatment rather than the generic sr::ui::DrawListView this screen drew before. The
// facility-integrity gauge moved out of this screen's own header into the router's top-bar Gauge
// (ActiveGaugeStatus below), the same move Repair's/Storage's own passes made. The click-a-row
// queue model is unchanged: a row's "RESEARCH" button is drawn, not a separately hit-tested
// widget -- Update() still hit-tests the whole row.
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
// whose MountId names this hardpoint) -- `Row::fill` carries progress/durationSeconds so the
// queue panel's own row treatment draws the per-row progress bar architecture.md 12.30.6 asks
// for.
std::vector<sr::ui::Row> QueueRows(const entt::registry& registry, entt::entity station,
                                   entt::entity facility);

// The player's own vessel (FactionRef == playerFaction) currently Docked at `station`, or
// entt::null if none. Duplicated locally per architecture.md 12.30's established rule (each
// docked-screen issue branches off main independently) rather than shared with the other
// screens' own copies.
entt::entity OwnedVesselAt(const entt::registry& registry, entt::entity station,
                           const FactionId& playerFaction);

// The mandatory per-screen facility-health readout (features.md 3.4) for the Research hardpoint
// PlayerLocation currently names, fed to the router's top-bar Gauge via SpaceFlight's
// orchestration (mirrors BayView.h's/StorageScreen.h's/RepairScreen.h's own ActiveGaugeStatus)
// rather than Research drawing its own in-page gauge any more (issue #227's visual-chrome pass).
// nullopt unless the screen is active and an owned vessel is docked there (the same gate Draw()
// itself uses).
std::optional<bridge_view::GaugeStatus> ActiveGaugeStatus(const entt::registry& registry,
                                                          const FactionId& playerFaction);

// Reads this frame's input and, while the player stands on a living Research hardpoint,
// hit-tests the CODEX button and the candidate list -- a click on an enabled row places a
// StartResearchRequest naming that item and this hardpoint's MountId on the requester. No-op on a
// disabled row.
void Update(entt::registry& registry, const FactionId& playerFaction,
            const core::knowledge::KnowledgeStore& knowledge);

// Draws the Research screen full-screen (architecture.md 12.30's frame; bridge_view::Draw already
// drew the one bezel around the whole window, so this does not draw its own, nor its own
// integrity gauge any more -- ActiveGaugeStatus above feeds that to the router's top bar
// instead): a fixed "RESEARCH LAB" header with a GRADE/SLOTS stat line, the CODEX button, and two
// bracket-bordered panels -- candidates, then the running queue -- each drawn with the bordered
// icon-box row treatment issue #227's visual-chrome pass matches to Bay's/Storage's/Repair's own.
// `fonts` is shared/ui/Fonts.h's Orbitron/Exo2 pair, replacing raylib's built-in bitmap font.
void Draw(const entt::registry& registry, const FactionId& playerFaction,
          const core::knowledge::KnowledgeStore& knowledge, const sr::ui::Fonts& fonts);

}  // namespace sr::space::ui::research_screen
