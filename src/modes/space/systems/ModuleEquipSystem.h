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
// requester's own Rig; the mount is Destroyed; the module's ModuleKind is not IsMountable() on
// the mount's ShellRole (mirrors Validation.h's ModuleCompatibility rule, applied live); the
// module is not in the requester's CargoHold; or (mount) MountedModules (shared/components/Rig.h
// -- the single record of a mount's contents, architecture.md 13.4 decision 2) is already
// non-empty -- unmount first.
//
// Scoped out: rig-wide BodyMass/Propulsion are not recomputed on mount/unmount. RigFactory's own
// engine-death handling is already documented as all-or-nothing rather than proportional
// (DamageSystem.h) for the same reason -- correctly re-aggregating a live rig's totals from a
// single hardpoint edit needs more bookkeeping than this issue's tested scope calls for.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::module_equip_system
