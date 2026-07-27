#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::collision_system {

// Rig-vs-rig physical collision. A spatial-grid broad phase (CollisionRadius circles, ported from
// StarReach2's SpatialGrid.h) rejects distant pairs cheaply; a convex-hull narrow phase (points
// sampled from each rig's living hardpoints, SAT-tested via StarReach2's CollisionHull.cpp math)
// resolves an asymmetric mass/momentum bounce and queues ramming damage on the hardpoint nearest
// the point of contact.
//
// Must run after PhysicsSystem (it reads this tick's settled WorldTransform/Velocity) and before
// DamageSystem (it queues PendingDamage for DamageSystem to resolve, same as ProjectileSystem).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::collision_system
