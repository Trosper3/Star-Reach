#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::npc_ai_system {

// Minimal opposition behaviour for the first vertical slice (architecture.md section 10, item 7):
// approach and fire. The full Patrol/Chase/Attack/Flee/Escort state machine from the section 4
// system inventory is out of scope here.
//
// Drives every rig root carrying Target, WorldTransform, and ThrustInput that is NOT
// PlayerControlled -- Law 4 means an NPC fighter and an NPC station are not special cases of each
// other, so a stationary rig with no Propulsion simply never moves even though this system still
// writes its ThrustInput, and still asks it to fire.
//
// Reads TargetingSystem's Target, so it must run after TargetingSystem. Writes FireIntent, which
// WeaponSystem drains and clears, so it must run before WeaponSystem. It does not aim -- that is
// WeaponSystem's FiringArc job -- it only decides whether the rig wants to close distance and
// whether it wants to shoot at all.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::npc_ai_system
