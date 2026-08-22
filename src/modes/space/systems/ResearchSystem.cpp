#include "modes/space/systems/ResearchSystem.h"

#include <algorithm>

#include "core/knowledge/KnowledgeNetwork.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/NetworkOwner.h"
#include "shared/components/Research.h"
#include "shared/components/Rig.h"
#include "shared/rig/ModuleAttachment.h"

namespace sr::space::research_system {
namespace {

constexpr float kBaseDurationSeconds = 60.0f;  // features.md 2.8: "60s base for a research job."

// features.md 2.4's settled table -- facility grade as a percentage of the base research time,
// indexed grade-1 (Common..Mythic, the same 1-7 ladder FacilityRef::grade already uses).
constexpr float kGradeTimeFactor[7] = {1.00f, 0.88f, 0.76f, 0.64f, 0.52f, 0.40f, 0.30f};

// A living (non-Destroyed) FacilityKind::Research hardpoint at `mount` on `station`, or
// entt::null. Per-job rather than per-station (architecture.md 12.30.8's amendment): a station
// may hold more than one lab, and destroying one must not freeze a sibling's jobs.
entt::entity ResolveResearchFacility(const entt::registry& registry, entt::entity station,
                                     const MountId& mount) {
    const entt::entity hardpoint = rig_attachment::FindHardpoint(registry, station, mount);
    if (hardpoint == entt::null || registry.all_of<Destroyed>(hardpoint)) {
        return entt::null;
    }
    const FacilityRef* facility = registry.try_get<FacilityRef>(hardpoint);
    if (facility == nullptr || facility->kind != FacilityKind::Research) {
        return entt::null;
    }
    return hardpoint;
}

// The docked station for `requester` -- the vessel entity itself (OwnedVesselAt's result, never
// PlayerControlled, which while standing on the Research hardpoint IS that hardpoint;
// RepairScreen.h documents the same trap). `requester` keeps carrying Docked regardless of where
// PlayerLocation currently points, so this needs none of BridgeView's PlayerLocation-based
// DockedStation fix. Duplicated locally rather than shared with StationServicesSystem's own copy
// on purpose -- neither system file has any other reason to depend on the other.
entt::entity DockedStationOf(const entt::registry& registry, entt::entity requester) {
    const Docked* docked = registry.try_get<Docked>(requester);
    if (docked == nullptr || !registry.valid(docked->station)) {
        return entt::null;
    }
    return docked->station;
}

void ProcessStartRequests(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> consumed;

    for (auto [self, request] : registry.view<StartResearchRequest>().each()) {
        // Consumed same-tick regardless of outcome -- the same BuyItemRequest/SellItemRequest
        // idiom this follows (architecture.md 12.30's "still consumed-same-tick" note).
        consumed.push_back(self);

        const entt::entity station = DockedStationOf(registry, self);
        const NetworkOwner* owner = registry.try_get<NetworkOwner>(self);
        if (station == entt::null || owner == nullptr) {
            continue;
        }
        const entt::entity hardpoint = ResolveResearchFacility(registry, station, request.facility);
        if (hardpoint == entt::null) {
            continue;  // No living Research hardpoint at the named mount.
        }

        // A refused request costs nothing and changes nothing -- StationFacility is not even
        // created until every check below has passed, the same "a refused build never costs
        // anything" rule ConstructionSystem documents.
        const StationFacility* existing = registry.try_get<StationFacility>(station);

        // architecture.md 12.30.8: "a station runs at most one job per item" -- also stops a
        // double-click from queuing the same research twice for no gain.
        const bool alreadyQueued =
            existing != nullptr &&
            std::any_of(existing->researchJobs.begin(), existing->researchJobs.end(),
                        [&](const ResearchJob& job) { return job.item == request.item; });
        if (alreadyQueued) {
            continue;
        }

        // architecture.md 12.30.6: "already-known items are refused before the click, not
        // after." The screen already disables the row with ALREADY KNOWN; this is that same
        // check's server-side half, the same defense-in-depth BuyItemRequest's affordability
        // check gets even though the screen also greys out what it can't afford.
        if (ctx.knowledge != nullptr) {
            const core::knowledge::KnowledgeNetwork* network = ctx.knowledge->Get(owner->network);
            if (network != nullptr && network->unlockedBlueprints.count(request.item.str()) != 0) {
                continue;
            }
        }

        const FacilityRef& facilityRef = registry.get<FacilityRef>(hardpoint);
        const int queued =
            existing != nullptr ? static_cast<int>(existing->researchJobs.size()) : 0;
        // FacilityStats::capacity's second reader (architecture.md 12.30.6): "how many units of
        // work a facility holds at once." 0 remains unlimited, matching DockingSystem's own.
        if (facilityRef.capacity != 0 && queued >= facilityRef.capacity) {
            continue;
        }

        ResearchJob job;
        job.item = request.item;
        job.durationSeconds = DurationSeconds(facilityRef.grade);
        job.targetNetwork = owner->network;
        job.facility = request.facility;
        registry.get_or_emplace<StationFacility>(station).researchJobs.push_back(std::move(job));
    }

    for (const entt::entity self : consumed) {
        registry.remove<StartResearchRequest>(self);
    }
}

}  // namespace

float DurationSeconds(int facilityGrade) {
    const int index = std::clamp(facilityGrade, 1, 7) - 1;
    return kBaseDurationSeconds * kGradeTimeFactor[index];
}

void Tick(const SystemContext& ctx) {
    ProcessStartRequests(ctx);

    entt::registry& registry = ctx.Registry();

    for (auto [station, facility] : registry.view<StationFacility>().each()) {
        auto it = facility.researchJobs.begin();
        while (it != facility.researchJobs.end()) {
            if (ResolveResearchFacility(registry, station, it->facility) == entt::null) {
                // This job's own lab is destroyed or gone -- frozen, same as a null grant below,
                // never dropped, and never advanced by a sibling lab surviving in its place.
                ++it;
                continue;
            }

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
        record.facility = job.facility;
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
        job.facility = record.facility;
        job.progress = std::min(record.durationSeconds, record.progress + elapsedSeconds);
        facility.researchJobs.push_back(std::move(job));
    }
}

}  // namespace sr::space::research_system
