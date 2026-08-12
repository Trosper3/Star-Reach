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
// Attach/Detach keep HardpointMass, EnginePropulsion and HardpointSensorRange (Rig.h) current on
// the hardpoint itself; RecomputeRigTotals below folds those cached per-hardpoint values into the
// rig-wide BodyMass/Propulsion/SensorRange, which is why it needs no ContentLibrary of its own
// (shared/ may not include core/, section 2.3) -- everything it reads was already written at
// mount time.
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

// The exact inverse: removes every role component AttachModuleComponents may have added for
// `module`, and subtracts its mass back out of HardpointMass. RigFactory never needs this -- a
// freshly built rig is never partially un-built -- but ModuleEquipSystem's unmount path does.
// Takes the full ModuleDef, not just its kind, because mass-subtraction needs module.mass.
void DetachModuleComponents(entt::registry& registry, entt::entity hardpoint,
                            const ModuleDef& module);

// Recomputes rigRoot's BodyMass, Propulsion and SensorRange from scratch, summing HardpointMass
// (Sum), EnginePropulsion (thrustNewtons/turnTorque summed, maxSpeed maxed) and
// HardpointSensorRange (Max) across every hardpoint in its Rig::children that is not Destroyed --
// architecture.md 12.23's Sum/Max rule. A no-op if rigRoot has no Rig. Callers: ModuleEquipSystem
// after every mount/unmount, and DamageSystem every tick a hardpoint may have just died, so
// losing an engine costs thrust proportionally instead of zeroing it only when the last one dies,
// and losing a sensor hardpoint shrinks detection range rather than leaving it stale.
void RecomputeRigTotals(entt::registry& registry, entt::entity rigRoot);

}  // namespace sr::rig_attachment
