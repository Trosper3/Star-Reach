#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

#include "shared/blueprints/ModuleDef.h"

// shared/rig/ -- the minimal attach/detach logic RigFactory::AttachModule and
// modes/space/systems/ModuleEquipSystem both need, extracted per architecture.md 12.11's Option
// 1 (recommended there): attaching a module to an already-instantiated rig is the same operation
// RigFactory performs at construction time, but modes/space/systems/ must not include
// factories/ (section 2.3). Pure, given a mount entity and a resolved ModuleDef -- neither side
// includes the other.
//
// What this does NOT do: fold mass or Propulsion into a rig-wide total. RigFactory accumulates
// those across every mount of a fresh build; a live single-mount equip/unequip is a different
// aggregation shape (recomputing one hardpoint's contribution against a rig that already has a
// settled BodyMass/Propulsion). That stays each caller's own job.
namespace sr::rig_attachment {

// A module's contribution to the rig-wide Propulsion total, if any (Law 4 -- thrust is a
// property of the rig, not the hardpoint). `present` is false for every non-Engine module.
struct PropulsionContribution {
    bool present = false;
    float thrustNewtons = 0.0f;
    float turnTorque = 0.0f;
    float maxSpeed = 0.0f;
};

// Attaches the role components implied by `module` onto `hardpoint` (Weapon/FiringArc,
// Shield, PowerSource/PowerLoad, FacilityRef/DockingBay) -- identical to what RigFactory writes
// at construction time. `mountTraverseRadians` only matters for ModuleKind::Weapon.
PropulsionContribution AttachModuleComponents(entt::registry& registry, entt::entity hardpoint,
                                              const ModuleDef& module, float mountTraverseRadians);

// The exact inverse: removes every role component AttachModuleComponents may have added for a
// module of this kind. RigFactory never needs this -- a freshly built rig is never partially
// un-built -- but ModuleEquipSystem's unmount path does.
void DetachModuleComponents(entt::registry& registry, entt::entity hardpoint, ModuleKind kind);

}  // namespace sr::rig_attachment
