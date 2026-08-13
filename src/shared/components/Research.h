#pragma once

#include <vector>

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// One reverse-engineering job (features.md 2.4, architecture.md 12.1's ResearchSystem
// subsection). `item` is the ModuleId being reverse-engineered -- there is no separate ItemId
// type in Ids.h yet, and a researched item always arrives as a collected ModuleId (the same type
// CargoHold::modules already carries), so reusing it avoids inventing a type with one caller.
// `durationSeconds` holds seconds, not credits -- named `cost` before architecture.md 13.5 group
// 2 / 12.30.6 caught the collision with every other `cost` field in the codebase (BuyItemRequest,
// RepairRequest), which are all credits. POD, section 11.4. Written by ResearchSystem only.
struct ResearchJob {
    ModuleId item;
    float progress = 0.0f;
    float durationSeconds = 0.0f;
    KnowledgeNetworkId targetNetwork;
};

// Per-station facility state for whatever reverse-engineering a station is running. A job is
// per-station state, not a rig, so Rig::children (Law 4) does not apply -- a station running two
// jobs at once holds both in this one vector.
//
// No tier/rate field here -- architecture.md 12.30.6 deleted `researchTier`, a bare float never
// written outside tests and a third, unreconciled tier system alongside `FacilityStats::level`
// and `Grade`. Progress-rate scaling by facility grade is future work (12.30.6's Grade-derived
// duration); ResearchSystem advances every living lab's jobs at a flat 1x for now.
struct StationFacility {
    std::vector<ResearchJob> researchJobs;
};

}  // namespace sr
