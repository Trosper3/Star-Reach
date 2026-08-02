#pragma once

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// architecture.md 12.10 -- buy/sell/repair at any station the requester is Docking.h's Docked
// at, including one they do not own. Set by input/UI on the docked rig root; consumed and
// cleared by StationServicesSystem the same tick, the same idiom as Docking.h's DockRequest.
//
// `cost`/`value`/`costForFullRepair` are supplied already-resolved rather than looked up from a
// price registry: no per-item pricing content exists anywhere in this codebase yet (ModuleDef
// carries no price field), the same content-schema gap architecture.md 12.7's TemplateMarketSystem
// left to its own caller-supplied basePayout.
struct BuyItemRequest {
    ModuleId module;
    int cost = 0;
};

struct SellItemRequest {
    ModuleId module;
    int value = 0;
};

struct RepairRequest {
    float fraction = 0.0f;
    int costForFullRepair = 0;
};

// architecture.md 12.10 also names MergeModuleRequest{ ModuleId a, b }, deliberately not defined
// here: it is blocked on a module tier/grade concept that does not exist anywhere in ModuleDef
// yet (the same kind of content-schema gap 12.11 flags for its own layering question). Buy/sell/
// repair do not depend on it and can land first -- see architecture.md 11.9's dependency table.

}  // namespace sr
