#pragma once

#include <entt/entity/entity.hpp>

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

// architecture.md 12.30.4: an ORDER, not a request -- a deliberate exception to the same-tick
// idiom every other intent in this file follows. Repair takes many ticks by construction, so
// this persists until met, stopped, undocked, or invalidated (its facility destroyed). Name it
// and comment it, or the next contributor "fixes" it into the same-tick pattern and silently
// makes repair instant again.
struct RepairOrder {
    // The rig actually being healed: the requester's own vessel, or the station it is standing
    // in when that station's FactionRef is the requester's own (architecture.md 12.30.3's
    // ownership test -- "a station with a repair bay repairs itself"). Lives on the docked
    // requester regardless of which one this names -- the same reason a screen must never place
    // a request on PlayerControlled (12.30.3's named trap): while docked inside a facility,
    // PlayerControlled is the station, not the vessel the player arrived in.
    entt::entity subject = entt::null;
    // entt::null means the whole rig ("Repair All"); otherwise one specific hardpoint of subject.
    entt::entity hardpoint = entt::null;
    // Stop once this hardpoint (or every hardpoint, for the whole-rig case) reaches this
    // fraction of its own max -- never above it, and never instantly regardless of value.
    float targetFraction = 1.0f;
    // Sub-credit carry: billing is whole credits per tick, and the fractional remainder that
    // would otherwise round to zero every tick -- making repair free again -- accumulates here.
    float creditRemainder = 0.0f;
};

// architecture.md 12.10 also names MergeModuleRequest{ ModuleId a, b }, deliberately not defined
// here: it is blocked on a module tier/grade concept that does not exist anywhere in ModuleDef
// yet (the same kind of content-schema gap 12.11 flags for its own layering question). Buy/sell/
// repair do not depend on it and can land first -- see architecture.md 11.9's dependency table.

}  // namespace sr
