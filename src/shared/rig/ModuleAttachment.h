#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

#include "shared/blueprints/Ids.h"
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

// ~30% of aggregate structural integrity (features.md 3.2's "a ship dies before its last
// hardpoint does") -- tunable in the 25-35% band per the same section, pending tools/economy_sim.
// Below this, DamageSystem destroys every surviving hardpoint and the rig with them; CockpitHud
// normalizes its displayed bar against it so the bar reaches a true zero at the failure point
// rather than at raw zero.
inline constexpr float kStructuralFailureThreshold = 0.30f;

// Sum of LIVING (non-Destroyed) hardpoint health over total hardpoint health across rigRoot's
// Rig::children (features.md 3.2's structural-integrity formula) -- derived, never stored. A
// hardpoint tagged Destroyed this tick contributes zero to the numerator regardless of whatever
// its own Health::current still reads (StructuralAttachment cascade tags Destroyed without
// zeroing Health), but its max still counts -- the rig's total capacity does not shrink because a
// piece of it died. Returns 0 for a rig with no Health-bearing hardpoints (a root with no
// children yet, not a crash). Pure -- no raylib, no UI type -- so both DamageSystem (destruction)
// and CockpitHud (the same aggregate's display) can read it without either depending on the other
// (architecture.md 2.3: modes/space/systems/ may not include sibling ui/, and ui/ may not include
// systems/).
float AggregateStructuralIntegrity(const entt::registry& registry, entt::entity rigRoot);

// Resolves a rig's hardpoint by its stable mount id, or entt::null. The (NetworkId, MountId)
// pair is how a hardpoint is addressed across a save or the wire -- an entt::entity never is.
// Lives here rather than RigFactory (its original home) because it is a lookup over an
// already-built rig, not construction: modes/space/systems/ may not include factories/ (section
// 2.3), but ResearchSystem needs exactly this to resolve a job back to its bench. RigFactory.h
// re-exports it under rig_factory:: for its own established callers.
entt::entity FindHardpoint(const entt::registry& registry, entt::entity root, const MountId& mount);

}  // namespace sr::rig_attachment
