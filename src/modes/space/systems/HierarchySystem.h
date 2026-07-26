#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::hierarchy_system {

// Propagates each rig root's WorldTransform to its hardpoints via LocalTransform (Law 4).
//
// Must run first in TickSchedule -- every later system reads a hardpoint's WorldTransform flat,
// with no parent lookup, and that value is stale until this pass has written it this tick.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::hierarchy_system
