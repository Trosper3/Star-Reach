#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::weapon_system {

// Fire control: aims each rig's live weapon hardpoints at an aim point, ticks their cooldowns,
// and spawns Projectile entities when a mount is aimed, in range, its weapon group enabled, and
// its rig has FireIntent set this tick. The aim point is the rig root's AimPoint if it has one --
// the player's cursor, features.md 3.2's "no target lock" -- else TargetingSystem's selected
// Target hardpoint.
//
// Must run after TargetingSystem (it reads Target) and before ProjectileSystem (which advances
// what this system spawns). Reads PowerBudget if the rig root has one -- a browned-out rig's
// guns recover from cooldown slower, the same scaling PhysicsSystem applies to thrust (Power.h).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::weapon_system
