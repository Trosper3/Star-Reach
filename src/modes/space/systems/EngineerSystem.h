#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::engineer_system {

// architecture.md 12.12's EngineerMenu: merge two owned modules of the same ModuleKind into one,
// at a level-scaled loss on the secondary module's contribution. Tier 1.
//
// Consumes every MergeModulesRequest (shared/components/Engineer.h) the same tick it's set, the
// same idiom DockingSystem already uses for DockRequest.
//
// Refused (request cleared, nothing else happens) when: the requester is not Docked; the docked
// station has no living (non-Destroyed) FacilityKind::Engineering hardpoint; `primary`/
// `secondary` do not both resolve via ctx.content, are not the same ModuleKind, or are not both
// present (as two distinct stacks, via shared/rig/CargoView.h) across the requester's own cargo
// bays; or ctx.craftedModules is null (no store to register the result into). If the merged
// result has nowhere to go, both originals are restored rather than lost.
//
// On success: registers a new ModuleDef via ctx.craftedModules->RegisterCraftedModule --
// core/registries/ContentLibrary.h's comment on that method explains why this is player-
// generated content, not authored content, the same distinction CustomizeMenu's Template draft
// already makes. Every numeric field on the merge (mass, powerDraw, powerGeneration, hullBonus,
// and whichever kind-specific stat block matches) becomes
// `primaryValue + secondaryValue * (engineerLevel * 0.1)` -- the Engineering facility's
// FacilityRef::level (1-5) scaling how much of the secondary module survives the merge. This is
// a placeholder scale, not a tuned value, the same category architecture.md 12.7's rate-roll
// weights are flagged as. Removes `primary` and `secondary` from CargoHold and adds the merged
// module's id.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::engineer_system
