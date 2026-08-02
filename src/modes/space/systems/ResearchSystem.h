#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <string>
#include <vector>

#include "core/galaxy/ResearchRecord.h"
#include "modes/space/systems/System.h"

namespace sr::space::research_system {

// Reverse-engineering jobs: cost, progress, unlock into a network (architecture.md 12.1's
// ResearchSystem subsection, features.md 2.4). Advances every resident station's
// StationFacility::researchJobs against dt and the facility's researchTier; on completion grants
// the researched item into its targetNetwork via ctx.knowledge (a no-op if ctx.knowledge is
// null, the same guard DiscoverySystem uses for ctx.discovery) and clears the job. No
// TickCoarse -- see CollapseResearchJobs/PromoteResearchJobs below for how a job survives a
// system demoting out of Tier 1 instead.
void Tick(const SystemContext& ctx);

// The demotion/promotion path for research jobs (architecture.md 12.5's precedent, applied here
// exactly): CollapseResearchJobs writes a durable core::galaxy::ResearchRecord for every job on
// `station` and clears StationFacility::researchJobs; PromoteResearchJobs re-instantiates jobs
// from records onto `station`, advancing each by `elapsedSeconds` at the station's current
// researchTier first -- "resumes at the progress it would have reached had the station stayed
// resident," not the frozen progress it had when the system demoted. Neither is on ctx's per-tick
// clock -- each is a one-shot conversion, called around whatever tears down or instantiates the
// entity's SystemWorld. `station` must carry StationFacility.
std::vector<core::galaxy::ResearchRecord> CollapseResearchJobs(entt::registry& registry,
                                                               entt::entity station,
                                                               const std::string& systemId);

void PromoteResearchJobs(entt::registry& registry, entt::entity station,
                         const std::vector<core::galaxy::ResearchRecord>& records,
                         float elapsedSeconds);

}  // namespace sr::space::research_system
