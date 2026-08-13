#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "shared/blueprints/Ids.h"

namespace sr::core::galaxy {

// Durable form of a ResearchJob (Law 3) -- architecture.md 12.1's ResearchSystem subsection,
// the same class of hole 12.5's WreckRecord resolved: a job only advances while its station's
// system is resident (Tier 1-2), and this is what a demoted system leaves behind so the job is
// not silently frozen or lost when the player warps away mid-research.
struct ResearchRecord {
    std::string systemId;
    ModuleId item;
    float progress = 0.0f;
    float durationSeconds = 0.0f;
    KnowledgeNetworkId targetNetwork;
};

// Galaxy-wide store of demoted research jobs (Law 8 -- core/galaxy/, not any one registry), the
// same "plain data outside any one registry" shape WreckLedger uses. Keyed by an id assigned at
// write time, since one station can run more than one job and one system can hold more than one
// station.
class ResearchLedger {
public:
    using Id = std::uint64_t;
    static constexpr Id kInvalidId = 0;

    // Stores `record` and returns the id it is later found or removed by.
    Id Add(ResearchRecord record);

    // No-op if `id` is unknown -- already promoted or otherwise cleared.
    void Remove(Id id);

    const ResearchRecord* Find(Id id) const;

    // Ids of every currently-stored record for `systemId`. Used by whatever re-instantiates a
    // system's research jobs when it becomes resident again.
    std::vector<Id> IdsForSystem(const std::string& systemId) const;

    std::size_t Count() const { return records_.size(); }

private:
    std::unordered_map<Id, ResearchRecord> records_;
    Id nextId_ = 1;
};

}  // namespace sr::core::galaxy
