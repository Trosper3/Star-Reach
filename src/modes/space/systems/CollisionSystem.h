#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::collision_system {

// Rig-vs-rig physical collision. A spatial-grid broad phase (CollisionRadius circles, ported from
// StarReach2's SpatialGrid.h) rejects distant pairs cheaply; a per-hardpoint narrow phase (every
// living hardpoint's own HitRadius circle, tested pairwise for the deepest-penetration pair --
// architecture.md 12.22, replacing an earlier convex-hull SAT so a destroyed hardpoint opens a
// real hole rather than one baked into a hull that lagged a tick behind death) resolves an
// asymmetric mass/momentum bounce and queues ramming damage, scaled by the pair's reduced mass,
// on the hardpoint nearest the point of contact.
//
// Must run after PhysicsSystem (it reads this tick's settled WorldTransform/Velocity) and before
// DamageSystem (it queues PendingDamage for DamageSystem to resolve, same as ProjectileSystem).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::collision_system
