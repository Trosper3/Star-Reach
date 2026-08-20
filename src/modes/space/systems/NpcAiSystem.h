#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::npc_ai_system {

// The opposition state machine (architecture.md section 4's NpcAiSystem inventory row,
// features.md section 3.2): Patrol, Chase, Attack, Flee, Escort. No new sensor concept --
// acquisition comes from TargetingSystem's Target, disengagement from a rig's own structural
// integrity, reach from SensorRange.
//
//   - Patrol: no Target and no AiBehavior::escortTarget assigned. Coasts -- there is no
//     galaxy-level waypoint/rally-point concept yet (architecture.md 12.6's NavigationMap, which
//     would supply one, is itself unbuilt).
//   - Chase / Attack: Target acquired. Chase closes distance once roughly aimed; Attack holds at
//     range once within kEngageRangeUnits. Both request FireIntent -- WeaponSystem's own per-
//     hardpoint Weapon::rangeUnits is what actually gates a shot landing, not this system. Beyond
//     the rig's own sensor reach the chase is dropped instead of continuing forever, and control
//     falls back to Patrol/Escort.
//   - Flee: this tick's aggregate structural integrity (shared/rig/ModuleAttachment.h) has
//     dropped below a threshold -- turns and burns away from the current Target instead of toward
//     it, and never requests FireIntent. The load-bearing state: it is what makes structural
//     damage frightening rather than arithmetic.
//   - Escort: no Target, but AiBehavior::escortTarget names a friendly rig to station-keep on.
//     Writes no PartyLeader/PartyMember of its own -- that pair belongs to a Defend order (P8-02),
//     not to this system.
//
// Drives every rig root carrying Target, WorldTransform, and ThrustInput that is NOT
// PlayerControlled -- Law 4 means an NPC fighter and an NPC station are not special cases of each
// other, so a stationary rig with no Propulsion simply never moves even though this system still
// writes its ThrustInput, and still asks it to fire.
//
// Reads TargetingSystem's Target, so it must run after TargetingSystem. Writes FireIntent, which
// WeaponSystem drains and clears, so it must run before WeaponSystem. It does not aim -- that is
// WeaponSystem's FiringArc job -- it only decides whether the rig wants to close distance, flee,
// station-keep, or shoot at all.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::npc_ai_system
