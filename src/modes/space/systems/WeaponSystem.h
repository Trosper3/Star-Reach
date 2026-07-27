#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::weapon_system {

// Fire control: aims each rig's live weapon hardpoints at TargetingSystem's selected aim point,
// ticks their cooldowns, and spawns Projectile entities when a mount is aimed, in range, and its
// rig has FireIntent set this tick.
//
// Must run after TargetingSystem (it reads Target) and before ProjectileSystem (which advances
// what this system spawns). Reads PowerBudget if the rig root has one -- a browned-out rig's
// guns recover from cooldown slower, the same scaling PhysicsSystem applies to thrust (Power.h).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::weapon_system
