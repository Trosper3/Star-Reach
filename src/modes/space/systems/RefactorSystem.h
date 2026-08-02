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
// hardpoint would orphan its children -- deletion is scoped to leaves); or the requester's
// CargoHold does not have room for every module the hardpoint returns
// (CargoHoldHasRoomFor, shared/components/Loot.h) -- "if storage is full, don't allow the
// deletion," refused whole rather than returning a partial set and dropping the rest.
//
// On success: every id in the hardpoint's MountedModules (shared/components/Rig.h) is appended
// to the requester's CargoHold, the hardpoint is removed from Rig::children, and the hardpoint
// entity is destroyed.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::refactor_system
