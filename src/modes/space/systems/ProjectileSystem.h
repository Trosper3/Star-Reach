#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::projectile_system {

// Advances every live Projectile along its straight-line path, resolves the first hardpoint hit
// along that path this tick (skipping the shooter's own rig), queues PendingDamage on it, and
// culls the projectile on either a hit or exhausting its remainingRange.
//
// Must run after WeaponSystem (it advances what that system just spawned this tick) and before
// DamageSystem (which drains the PendingDamage this system writes).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::projectile_system
