#include "modes/space/systems/ResearchSystem.h"

#include <algorithm>

#include "shared/components/Facility.h"
#include "shared/components/Research.h"
#include "shared/components/Rig.h"

namespace sr::space::research_system {
namespace {

// A living (non-Destroyed) FacilityKind::Research hardpoint on `station` -- the same gate shape
// EngineerSystem/RefactorSystem/StationServicesSystem use for their own facility kinds.
// Duplicated locally rather than shared: it is a dozen lines, and none of those systems have any
// other reason to depend on this one.
bool HasLivingResearchFacility(const entt::registry& registry, entt::entity station) {
    const Rig* rig = registry.try_get<Rig>(station);
    if (rig == nullptr) {
        return false;
    }
    for (const entt::entity hardpoint : rig->children) {
        if (registry.all_of<Destroyed>(hardpoint)) {
            continue;
        }
        const FacilityRef* facility = registry.try_get<FacilityRef>(hardpoint);
        if (facility != nullptr && facility->kind == FacilityKind::Research) {
            return true;
        }
    }
    return false;
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    for (auto [station, facility] : registry.view<StationFacility>().each()) {
        if (!HasLivingResearchFacility(registry, station)) {
            continue;  // Blowing the lab off the station freezes its jobs, same as a null grant.
        }

        auto it = facility.researchJobs.begin();
        while (it != facility.researchJobs.end()) {
            it->progress += ctx.dt;

            if (it->progress < it->durationSeconds) {
                ++it;
                continue;
            }

            if (ctx.knowledge == nullptr) {
                // A null-pointer guard may only skip an effect that is free to skip -- the grant
                // is destructive (it IS the point), so freeze at completion instead of erasing
                // the job and silently spending the input for nothing.
                it->progress = it->durationSeconds;
                ++it;
                continue;
            }

            ctx.knowledge->Grant(it->targetNetwork,
                                 core::knowledge::NetworkEntryKind::UnlockedBlueprint,
                                 it->item.str());
            it = facility.researchJobs.erase(it);
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
        record.durationSeconds = job.durationSeconds;
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
        job.durationSeconds = record.durationSeconds;
        job.targetNetwork = record.targetNetwork;
        job.progress = std::min(record.durationSeconds, record.progress + elapsedSeconds);
        facility.researchJobs.push_back(std::move(job));
    }
}

}  // namespace sr::space::research_system
