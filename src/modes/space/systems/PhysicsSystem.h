#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::physics_system {

// Integrates rig-root motion: ThrustInput + Propulsion become a force, BodyMass turns force into
// acceleration (real F = ma, features.md section 2.2), LinearDamping bleeds it off, and the
// result is written back into WorldTransform.
//
// Must run after HierarchySystem (it writes the root WorldTransform hardpoints then read) and
// before anything that reads a hardpoint's position this tick.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::physics_system
