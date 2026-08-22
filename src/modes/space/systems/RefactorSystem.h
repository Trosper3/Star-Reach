#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::refactor_system {

// architecture.md 12.30.5's Engineering screen: Delete and Rebuild, the rig-facing half of its
// two axes (EngineerSystem owns Merge/Deconstruct, the CargoHold-facing half, behind the same
// shared/rig/DockedFacility.h gate this file used to duplicate locally). Tier 1.
//
// Consumes every DeleteHardpointRequest and RebuildMountRequest (shared/components/Refactor.h)
// the same tick either is set, the same idiom DockingSystem already uses for DockRequest.
//
// Delete -- refused (request cleared, nothing else happens) when: the requester is not currently
// standing in a living FacilityKind::Engineering hardpoint (docked_facility::DockedFacility); the
// named hardpoint does not belong to the requester's own Rig; another hardpoint's
// StructuralAttachment still points at it (a non-leaf hardpoint would orphan its children --
// deletion is scoped to leaves); or (features.md 2.2's settled reversal, architecture.md 15.2
// finding 8) the hardpoint still holds modules and is not Destroyed -- unmount first, then
// delete. On success: the hardpoint is removed from Rig::children and the hardpoint entity is
// destroyed. Nothing is ever refunded to cargo here -- ModuleEquipSystem's unmount path already
// did that, or (a Destroyed hardpoint) there is nothing left to refund.
//
// Rebuild -- the delete, inverted (architecture.md 12.30.5). Refused under the same facility
// gate, or when: `mount` does not name a MountBlueprint on the requester's own BlueprintRef; that
// mount already has a living or Destroyed entity in Rig::children (rebuild only fills a true gap
// -- a Destroyed hardpoint must be Deleted first); or its authored `attachedTo` parent is missing
// or Destroyed (you cannot hang a wing off a hull that is not there -- delete works leaves-inward,
// rebuild works root-outward, one graph, two opposite refusals). On success: shared/rig/
// ModuleAttachment.h's CreateBareHardpoint adds a bare hardpoint (no modules -- "a rebuilt mount
// comes back bare," so a delete-then-rebuild cycle never duplicates what delete already refunded)
// to Rig::children, and RecomputeRigTotals folds its mass/propulsion/sensor contribution back in.
// Costs nothing yet -- architecture.md 12.30.5: a flat placeholder pending §12.19's Recipe-based
// cost, the same honesty this verb's mechanism does not need to wait on.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::refactor_system
