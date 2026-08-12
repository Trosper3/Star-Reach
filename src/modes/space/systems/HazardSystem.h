#pragma once

#include "modes/space/systems/System.h"

// modes/space/systems/ -- queues environmental PendingDamage from hazard volumes. Today that is
// exactly one hazard -- a star's Corona (shared/components/Physics.h) -- and it is its own file
// rather than folded into OrbitSystem because OrbitSystem owns orbits and gravity, not damage,
// and because nebulae, radiation belts and minefields are all the same shape.
//
// Schedule position -- two constraints, no fixed index:
//   After  HierarchySystem            -- reads settled hardpoint WorldTransforms.
//   Before CollisionSystem/ProjectileSystem -- both QueueDamage helpers overwrite
//                                       PendingDamage::source when accumulating; running the
//                                       hazard first means a real attacker's source always wins
//                                       the tick.
namespace sr::space::hazard_system {

void Tick(const SystemContext& ctx);

}  // namespace sr::space::hazard_system
