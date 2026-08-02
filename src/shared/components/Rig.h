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

}  // namespace sr
