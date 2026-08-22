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
// RepairRequest), which are all credits. `facility` is the Research hardpoint's stable MountRef::
// id (architecture.md 12.30.8's amendment to this section) -- StationFacility is per-*station*,
// so without it a job cannot say which of a station's (potentially several) labs it is running
// on, and the facility gate, the grade-derived duration and the capacity check below are all
// unevaluable on a station with two labs. POD, section 11.4. Written by ResearchSystem only.
struct ResearchJob {
    ModuleId item;
    float progress = 0.0f;
    float durationSeconds = 0.0f;
    KnowledgeNetworkId targetNetwork;
    MountId facility;
};

// architecture.md 12.30.6's missing entry point: nothing in src/ produces a ResearchJob today, so
// a mechanism that is otherwise fully built (advance, demote, promote, grant) can never start.
// Placed on the requester -- the docked vessel entity `OwnedVesselAt` resolves, never
// PlayerControlled, which while standing on the Research hardpoint is the hardpoint itself
// (RepairScreen.h documents the same trap) -- the same same-tick-request idiom BuyItemRequest/
// SellItemRequest use. `targetNetwork` is deliberately absent: the system resolves "the actor's
// network" itself from the requester's own NetworkOwner rather than trusting a client-supplied
// target, the same reason BuyItemRequest never carries a price.
struct StartResearchRequest {
    ModuleId item;
    MountId facility;
};

// Per-station facility state for whatever reverse-engineering a station is running. A job is
// per-station state, not a rig, so Rig::children (Law 4) does not apply -- a station running two
// jobs at once holds both in this one vector.
//
// No tier/rate field here -- architecture.md 12.30.6 deleted `researchTier`, a bare float never
// written outside tests and a third, unreconciled tier system alongside `FacilityStats::level`
// and `Grade`. Progress-rate scaling by facility grade now happens at job creation instead
// (ResearchSystem's DurationSeconds derives `durationSeconds` from the named hardpoint's
// FacilityRef::grade against features.md 2.4's settled table) -- Tick itself still just advances
// `progress` against `dt`, uniformly, whatever the job's duration came out to.
struct StationFacility {
    std::vector<ResearchJob> researchJobs;
};

}  // namespace sr
