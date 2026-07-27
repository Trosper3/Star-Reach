#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::power_system {

// Recomputes each rig root's PowerBudget from its living hardpoints' PowerSource/PowerLoad, then
// gates the load: under 100% draw the rig runs at full satisfaction; a mild overdraw (up to 125%
// of generation) throttles satisfaction proportionally; a severe overdraw sheds low-priority
// loads (ascending PowerLoad::priority -- facilities, then shields, then weapons, then engines)
// until the remainder fits the throttle band, tagging shed hardpoints with PowerShed.
//
// Must run before WeaponSystem and PhysicsSystem, the two systems that scale their output by
// PowerBudget::satisfaction (architecture.md section 11.3 point 4).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::power_system
