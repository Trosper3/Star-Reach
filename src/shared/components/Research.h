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
// POD, section 11.4. Written by ResearchSystem only.
struct ResearchJob {
    ModuleId item;
    float progress = 0.0f;
    float cost = 0.0f;
    KnowledgeNetworkId targetNetwork;
};

// Per-station facility state for whatever reverse-engineering a station is running. A job is
// per-station state, not a rig, so Rig::children (Law 4) does not apply -- a station running two
// jobs at once holds both in this one vector. `researchTier` scales ResearchSystem's progress
// rate (features.md 2.4's "facility-tier" cost factor); 1.0 is the baseline tier.
struct StationFacility {
    std::vector<ResearchJob> researchJobs;
    float researchTier = 1.0f;
};

}  // namespace sr
