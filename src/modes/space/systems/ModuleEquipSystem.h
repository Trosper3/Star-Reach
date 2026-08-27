#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::module_equip_system {

// Mount/unmount modules onto an already-live rig's hardpoints (architecture.md 12.11), Tier 1.
//
// Resolves every pending MountModuleRequest/UnmountModuleRequest (shared/components/Equip.h) the
// same tick they're set, the same idiom DockingSystem already uses for DockRequest. Attaching a
// module reuses shared/rig/ModuleAttachment.h's AttachModuleComponents/DetachModuleComponents --
// the Option 1 resolution architecture.md 12.11 recommends for the layering question it raises
// (equip-from-storage is the same operation RigFactory::AttachModule performs at construction,
// but modes/space/systems/ must not include factories/, section 2.3).
//
// Refused (request cleared, nothing else happens) when: the mount does not belong to the
// requester's own Rig; the mount is Destroyed; the module's ModuleKind fails the mount's
// ShellRole::Accepts() (mirrors Validation.h's ModuleCompatibility rule, applied live); the
// module is not held anywhere in the source's own cargo bays (shared/rig/CargoView.h --
// MountModuleRequest::sourceCargo when set, else the mount's own rig); or (mount) MountedModules
// (shared/components/Rig.h -- the single record of a mount's contents, architecture.md 13.4
// decision 2) is already non-empty -- unmount first. Unmounting is refused the same way if the
// destination cargo bays (UnmountModuleRequest::destinationCargo when set, else the mount's own
// rig) have nowhere to take the module back. The source/destination fields are Engineering's own
// cross-hull addition -- entt::null (ModulesMenu's own single-hull mount/unmount never sets them)
// keeps this exactly ModulesMenu's original single-hull behavior.
//
// rig_attachment::RecomputeRigTotals runs after every successful mount/unmount, so BodyMass/
// Propulsion/SensorRange stay current with a live edit, not just a fresh build or a hardpoint
// death (architecture.md 12.23).
//
// Unmounting a CargoBay-kind module spills its own contents as recoverable drops first
// (modes/space/systems/LootSystem.h's SpillCargoHold) -- the same path a destroyed bay takes.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::module_equip_system
