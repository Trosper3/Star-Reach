#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::engineer_system {

// architecture.md 12.30.5's Engineering screen: Merge and Deconstruct, the CargoHold-facing half
// of its two axes (RefactorSystem owns Delete/Rebuild, the rig-facing half, behind the same
// shared/rig/DockedFacility.h gate). Tier 1.
//
// Consumes every MergeModulesRequest and DeconstructModuleRequest (shared/components/Engineer.h)
// the same tick either is set, the same idiom DockingSystem already uses for DockRequest.
//
// Merge -- refused (request cleared, nothing else happens) when: the requester is not currently
// standing in a living FacilityKind::Engineering hardpoint (docked_facility::DockedFacility);
// `primary`/`secondary` do not both resolve via ctx.content, are not the same ModuleKind, or are
// not both present (as two distinct stacks, via shared/rig/CargoView.h) across the requester's
// own cargo bays; or ctx.craftedModules is null (no store to register the result into). If the
// merged result has nowhere to go, both originals are restored rather than lost.
//
// On a successful merge: registers a new ModuleDef via ctx.craftedModules->RegisterCraftedModule
// -- core/registries/ContentLibrary.h's comment on that method explains why this is player-
// generated content, not authored content, the same distinction CustomizeMenu's Template draft
// already makes. Every numeric field on the merge (mass, powerDraw, powerGeneration, hullBonus,
// and whichever kind-specific stat block matches) becomes
// `primaryValue + secondaryValue * (engineerLevel * 0.1)` -- the Engineering facility's
// FacilityRef::grade scaling how much of the secondary module survives the merge. This is a
// placeholder scale, not a tuned value, the same category architecture.md 12.7's rate-roll
// weights are flagged as. Removes `primary` and `secondary` from CargoHold and adds the merged
// module's id. Deliberately not routed to a screen yet (architecture.md 12.30.5): unbounded and
// free until §12.21's Quality exists to cap it.
//
// Deconstruct -- refused under the same facility gate, or when `module` does not resolve via
// ctx.content or is not present in the requester's own cargo bays. On success: removes it from
// CargoHold and credits the requester's own Wallet (if any) with a flat, grade-scaled placeholder
// -- see Engineer.h's DeconstructModuleRequest comment for why real material recovery waits on
// architecture.md 12.19's Recipe.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::engineer_system
