#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::orbit_system {

// Two independent passes, both driven by `shared/components/Orbit.h`:
//
//   1. Every OrbitBody's WorldTransform is set directly from `phase + angularSpeed * elapsed`,
//      resolved recursively through its center chain (planet around the sun, moon around planet).
//      This is a pure function of elapsed tick time, so it never drifts and can be evaluated at
//      any tick gap without replaying intermediate ones.
//   2. Any GravityWell pulls nearby Velocity-driven bodies (ships) toward it. Bodies with
//      OrbitBody are excluded -- pass 1 already fully determines their position.
//
// Order relative to the rest of the schedule: after HierarchySystem/PhysicsSystem write hardpoint
// and ship transforms for the tick, before anything that reads a ship's Velocity as final for the
// tick (there is nothing downstream of PhysicsSystem that does yet, but DamageSystem must stay
// last regardless).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::orbit_system
