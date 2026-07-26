#pragma once

#include "shared/math/Vec2.h"

namespace sr {

// Where a hardpoint sits relative to its rig root. Copied verbatim from
// MountBlueprint::localOffset/localRotation at instantiation and not written again unless the
// rig is refitted. Rig roots do not carry this component.
struct LocalTransform {
    Vec2 offset;
    float rotation = 0.0f;
};

// Absolute position in the owning system's registry.
//
// For a rig root this is authoritative -- PhysicsSystem writes it. For a hardpoint it is
// derived: HierarchySystem recomputes it once per tick from the root's WorldTransform and the
// child's LocalTransform. Every later system reads only this, flat, with no parent lookup.
// That single pass is the cost Law 4 budgets for the entity-per-hardpoint model.
//
// Nothing except PhysicsSystem (roots) and HierarchySystem (children) may write this.
struct WorldTransform {
    Vec2 position;
    float rotation = 0.0f;
};

// Render-side smoothing. Simulation ticks at a fixed 60 Hz (architecture.md section 5) while
// rendering runs free, so the renderer interpolates between the previous and current transform
// rather than reading raw simulation state and stuttering on frame spikes.
struct PreviousTransform {
    Vec2 position;
    float rotation = 0.0f;
};

}  // namespace sr
