#pragma once

#include <string>
#include <vector>

#include "shared/blueprints/Ids.h"
#include "shared/math/Vec2.h"

namespace sr {

// One authored hardpoint: a shell, where it sits, and what is bolted into it.
struct MountBlueprint {
    MountId id;  // Unique within one rig. Survives saves; lets a Template be edited in place.
    ShellId shell;
    std::vector<ModuleId> modules;

    // Position and facing RELATIVE TO THE RIG ROOT -- not relative to `attachedTo`.
    //
    // This is the load-bearing detail of Law 4's performance argument. Because every mount's
    // local transform is root-relative, HierarchySystem is a single flat pass: read the root's
    // WorldTransform, write each child's. If offsets chained through `attachedTo` instead, the
    // pass would become a recursive walk with random access into another pool, which is the
    // cost Law 4 explicitly budgets against.
    Vec2 localOffset;
    float localRotation = 0.0f;

    // Structural adjacency, used by validation (no floating islands) and by damage propagation
    // -- NOT by transform math. Empty on exactly one mount: the rig root.
    MountId attachedTo;

    // Firing arc for weapon shells, in radians, centered on localRotation. Zero or negative
    // means a fixed forward mount with no traverse.
    float traverseRadians = 0.0f;
};

// The composition of a craft or station, independent of which chassis stats apply. Kept separate
// from ShipBlueprint so a station, a capital, and a fighter are the same kind of data (Law 4) --
// this is the type whose absence in StarReach2 produced four different hardpoint wire formats.
struct RigBlueprint {
    std::vector<MountBlueprint> mounts;

    const MountBlueprint* Find(const MountId& id) const {
        for (const auto& mount : mounts) {
            if (mount.id == id) {
                return &mount;
            }
        }
        return nullptr;
    }
};

}  // namespace sr
