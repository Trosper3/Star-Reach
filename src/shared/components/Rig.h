#pragma once

#include <entt/entity/entity.hpp>
#include <vector>

#include "shared/blueprints/Ids.h"
#include "shared/blueprints/Taxonomy.h"

namespace sr {

// On a rig root: the hardpoint entities it owns.
//
// This is the one place a std::vector in a component is correct. Law 4 bans vectors that stand
// in for child entities; this vector *is* the list of child entities. Destroying the root
// destroys everything in it.
struct Rig {
    std::vector<entt::entity> children;
};

// On a hardpoint: the rig root it belongs to. Always valid while the hardpoint exists -- systems
// that destroy a root must destroy its children in the same tick, never leave dangling parents.
struct ParentRig {
    entt::entity root = entt::null;
};

// What this hardpoint structurally is. Copied from ShellDef::kind so damage, power, and
// targeting can filter without a registry lookup back into authored data every frame.
struct ShellRole {
    ShellKind kind = ShellKind::Armor;

    // Copied from ShellDef::acceptsKinds at instantiation (architecture.md 12.22) -- a live
    // refit (ModuleEquipSystem) only ever has the hardpoint in hand, never the authored ShellDef,
    // the same reason `kind` above is copied rather than looked up. Armor is not listed here: see
    // ShellDef::Accepts's comment; Accepts() below checks it separately.
    std::vector<ModuleKind> acceptsKinds;

    bool Accepts(ModuleKind moduleKind) const {
        if (moduleKind == ModuleKind::Armor) {
            return true;
        }
        for (const ModuleKind accepted : acceptsKinds) {
            if (accepted == moduleKind) {
                return true;
            }
        }
        return false;
    }
};

// Per-hardpoint hit radius, copied from ShellDef::radius at instantiation. ProjectileSystem's
// hit test reads this. CollisionRadius (Physics.h) is a different granularity entirely -- the
// rig-root broad-phase circle, not a per-hardpoint narrow-phase one.
struct HitRadius {
    float value = 0.0f;
};

// Structural adjacency from MountBlueprint::attachedTo, resolved to a handle.
//
// Deliberately separate from ParentRig. ParentRig is the flat root link that HierarchySystem
// uses; this is the structural graph that damage propagation and refit validation use. They are
// different relationships and collapsing them is what makes transform propagation recursive.
struct StructuralAttachment {
    entt::entity attachedTo = entt::null;
};

// Tag: this hardpoint's hull reached zero. Its capability is gone permanently for this rig
// (features.md section 3.2) -- destroy the thruster and the ship stalls.
//
// A tag rather than a bool inside Health, so systems iterate only live hardpoints via
// entt exclusion instead of branching per entity.
struct Destroyed {};

// Which ModuleId(s) RigFactory attached to this hardpoint at construction time. Written once, at
// spawn, by RigFactory::CreateHardpoint. RefactorSystem reads this to know which modules a
// deleted hardpoint returns to CargoHold (architecture.md 12.12).
struct MountedModules {
    std::vector<ModuleId> ids;
};

// The mount's authored traverse limit, copied from MountBlueprint::traverseRadians at
// instantiation (RigFactory::CreateHardpoint) -- the same value RigFactory itself passes to
// AttachModuleComponents at build time. ModuleEquipSystem reads this so a live-refitted weapon
// gets the mount's real arc instead of a hardcoded 0.0f, which produced a FiringArc with zero
// width that could never satisfy AimAt's withinArc test (architecture.md 13.3 finding D).
struct MountTraverse {
    float radians = 0.0f;
};

// This hardpoint's own contribution to the rig's total BodyMass: the shell's mass, seeded by
// RigFactory::CreateHardpoint, plus whatever module is currently mounted on top (kept current by
// shared/rig/ModuleAttachment's Attach/Detach pair). RecomputeRigTotals sums this across every
// living hardpoint (architecture.md 12.23's Sum rule) -- the same "baked-in shell, live module"
// split HitRadius already establishes for per-hardpoint radius.
struct HardpointMass {
    float value = 0.0f;
};

// This hardpoint's own contribution to the rig's aggregate Propulsion, present only while it
// carries an Engine-kind module (shared/rig/ModuleAttachment.h's Attach/Detach pair owns this
// component's lifetime). RecomputeRigTotals sums thrustNewtons/turnTorque and maxes maxSpeed
// across living hardpoints carrying one -- architecture.md 12.23's Sum/Max rule -- which is what
// makes losing one of several engines cost thrust proportionally instead of all at once.
struct EnginePropulsion {
    float thrustNewtons = 0.0f;
    float turnTorque = 0.0f;
    float maxSpeed = 0.0f;
};

// This hardpoint's own contribution to the rig's SensorRange, present only while it carries a
// Sensor-kind module. RecomputeRigTotals maxes this across living hardpoints (architecture.md
// 12.23) -- two sensor arrays do not see twice as far, the same reasoning as EnginePropulsion's
// maxSpeed.
struct HardpointSensorRange {
    float value = 0.0f;
};

// Present on a hardpoint mounting a FireControl module: the module's authored automated-tracking
// rate. shared/rig/ModuleAttachment's Attach/Detach pair applies it directly to the co-mounted
// Weapon's FiringArc::turnRatePerSecond, regardless of which of the two modules a mount's
// authored list names first (architecture.md 12.23). Purely per-hardpoint, never rig-aggregated
// -- a FireControl module only ever helps the turret it shares a mount with.
struct FireControl {
    float turnRatePerSecond = 0.0f;
};

}  // namespace sr
