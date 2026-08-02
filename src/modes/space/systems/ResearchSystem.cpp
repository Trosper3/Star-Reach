#include "modes/space/systems/ResearchSystem.h"

#include <algorithm>

#include "shared/components/Research.h"

namespace sr::space::research_system {

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    for (auto [entity, facility] : registry.view<StationFacility>().each()) {
        (void)entity;
        auto it = facility.researchJobs.begin();
        while (it != facility.researchJobs.end()) {
            it->progress += ctx.dt * facility.researchTier;

            if (it->progress >= it->cost) {
                if (ctx.knowledge != nullptr) {
                    ctx.knowledge->Grant(it->targetNetwork,
                                         core::knowledge::NetworkEntryKind::UnlockedBlueprint,
                                         it->item.str());
                }
                it = facility.researchJobs.erase(it);
            } else {
                ++it;
            }
        }
    }
}

std::vector<core::galaxy::ResearchRecord> CollapseResearchJobs(entt::registry& registry,
                                                               entt::entity station,
                                                               const std::string& systemId) {
    std::vector<core::galaxy::ResearchRecord> records;

    StationFacility& facility = registry.get<StationFacility>(station);
    records.reserve(facility.researchJobs.size());
    for (const ResearchJob& job : facility.researchJobs) {
        core::galaxy::ResearchRecord record;
        record.systemId = systemId;
        record.item = job.item;
        record.progress = job.progress;
        record.cost = job.cost;
        record.targetNetwork = job.targetNetwork;
        records.push_back(std::move(record));
    }
    facility.researchJobs.clear();

    return records;
}

void PromoteResearchJobs(entt::registry& registry, entt::entity station,
                         const std::vector<core::galaxy::ResearchRecord>& records,
                         float elapsedSeconds) {
    StationFacility& facility = registry.get<StationFacility>(station);

    for (const core::galaxy::ResearchRecord& record : records) {
        ResearchJob job;
        job.item = record.item;
        job.cost = record.cost;
        job.targetNetwork = record.targetNetwork;
        job.progress =
            std::min(record.cost, record.progress + elapsedSeconds * facility.researchTier);
        facility.researchJobs.push_back(std::move(job));
    }
}

}  // namespace sr::space::research_system
