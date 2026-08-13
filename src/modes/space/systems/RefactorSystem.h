#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::refactor_system {

// architecture.md 12.12's RefactorMenu: delete a hardpoint from the requester's own live rig
// while docked somewhere with a living Engineering facility (the same gate EngineerSystem uses).
// Tier 1.
//
// Consumes every DeleteHardpointRequest (shared/components/Refactor.h) the same tick it's set,
// the same idiom DockingSystem already uses for DockRequest.
//
// Refused (request cleared, nothing else happens) when: the requester is not Docked at a station
// with a living FacilityKind::Engineering hardpoint; the named hardpoint does not belong to the
// requester's own Rig; another hardpoint's StructuralAttachment still points at it (a non-leaf
// hardpoint would orphan its children -- deletion is scoped to leaves); or (features.md 2.2's
// settled reversal, architecture.md 15.2 finding 8) the hardpoint still holds modules -- unmount
// first, then delete. Nothing is ever refunded to cargo here.
//
// On success: the hardpoint is removed from Rig::children and the hardpoint entity is destroyed.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::refactor_system
