#pragma once

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// architecture.md 12.12's EngineerMenu: merge two owned modules of the same ModuleKind into one,
// at a level-scaled loss on the secondary module's contribution. Set by input/UI on the docked
// rig root; consumed and cleared by EngineerSystem the same tick, the same idiom as Docking.h's
// DockRequest. Both ids must be present in the requester's own CargoHold.
//
// `primary` is kept as the merge's base; `secondary` contributes a fraction of its own stats,
// scaled by the docked Engineering facility's FacilityRef::level (1-5): contribution =
// secondaryStat * (level * 0.1) -- a level-1 engineer preserves 10% of the secondary module,
// level 5 preserves 50%. This is a placeholder scale, not a tuned value, the same category
// architecture.md 12.7's rate-roll weights are flagged as.
struct MergeModulesRequest {
    ModuleId primary;
    ModuleId secondary;
};

}  // namespace sr
