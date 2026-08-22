#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <string>
#include <vector>

#include "core/galaxy/ResearchRecord.h"
#include "modes/space/systems/System.h"

namespace sr::space::research_system {

// features.md 2.4's settled table -- facility grade sets research time as a percentage of a 60s
// base (features.md 2.8), Common (grade 1) at 100% down to Mythic (grade 7) 30%. Public so
// modes/space/ui/ResearchScreen.cpp (which may not include this header's Tick/System.h, only the
// gameplay data it displays) can duplicate the same formula for its pre-commit preview -- the
// same small-helper-duplication modes/space/ui/RepairScreen.cpp's OwnedVesselAt already accepts,
// since a UI file may not depend on a systems/ file (architecture.md section 2.3).
float DurationSeconds(int facilityGrade);

// Reverse-engineering jobs: durationSeconds, progress, unlock into a network (architecture.md
// 12.1's ResearchSystem subsection, features.md 2.4, revised by 12.30.6's defect pack). Gains, on
// top of the built advance/demote/promote/grant mechanism: the missing entry point
// (StartResearchRequest, placed on the requester by modes/space/ui/ResearchScreen.cpp, the same
// same-tick-request idiom BuyItemRequest uses) and its two refusals -- no living FacilityKind::
// Research hardpoint at the named MountId, or the target network already holds the item's unlock
// (architecture.md 12.30.6: "already-known items are refused before the click, not after," this
// system's own defense-in-depth half of a check the screen also makes before ever emitting the
// request) -- and the FacilityRef::capacity concurrent-job cap (0 unlimited, the same "how many
// units of work this facility holds at once" meaning DockingSystem's own capacity reader shares).
//
// Advances every resident station's StationFacility::researchJobs against dt, gated per-job on
// its own named MountId naming a living (non-Destroyed) FacilityKind::Research hardpoint --
// destroying that specific lab freezes only its own jobs, never a sibling lab's, in place rather
// than letting them keep completing. On completion grants the researched item into its
// targetNetwork via ctx.knowledge; a null ctx.knowledge is NOT the plain skip-the-tick guard
// DiscoverySystem uses on the same pointer -- ResearchSystem's grant is destructive of a job in
// progress, so a null pointer instead freezes the job at durationSeconds (resumable, input
// intact) rather than erasing it and silently spending the input for nothing. DiscoverySystem has
// nothing to freeze -- its scan is a pure re-derivable presence check, so a null ctx.knowledge is
// a harmless no-op tick. No TickCoarse -- see CollapseResearchJobs/PromoteResearchJobs below for
// how a job survives a system demoting out of Tier 1 instead.
void Tick(const SystemContext& ctx);

// The demotion/promotion path for research jobs (architecture.md 12.5's precedent, applied here
// exactly): CollapseResearchJobs writes a durable core::galaxy::ResearchRecord for every job on
// `station` and clears StationFacility::researchJobs; PromoteResearchJobs re-instantiates jobs
// from records onto `station`, advancing each by `elapsedSeconds` -- "resumes at the progress it
// would have reached had the station stayed resident," not the frozen progress it had when the
// system demoted. Neither is on ctx's per-tick clock -- each is a one-shot conversion, called
// around whatever tears down or instantiates the entity's SystemWorld. `station` must carry
// StationFacility.
std::vector<core::galaxy::ResearchRecord> CollapseResearchJobs(entt::registry& registry,
                                                               entt::entity station,
                                                               const std::string& systemId);

void PromoteResearchJobs(entt::registry& registry, entt::entity station,
                         const std::vector<core::galaxy::ResearchRecord>& records,
                         float elapsedSeconds);

}  // namespace sr::space::research_system
