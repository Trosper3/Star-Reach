#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::targeting_system {

// Maintains every seeker's Target: which hostile rig it is engaging, and which of that rig's
// living hardpoints it is aiming at (features.md section 3.2).
//
// A seeker is any entity carrying Target, WorldTransform, FactionRef, and SensorRange -- the
// player's rig and NPC rigs alike, since Law 4 means neither is a special case. Acquisition and
// re-acquisition both go through this system rather than through whatever wrote the Target, so a
// hardpoint dying mid-fight is noticed the same tick regardless of who set the target in the
// first place.
//
// Must run after HierarchySystem/PhysicsSystem (it reads WorldTransform) and before WeaponSystem
// (which aims at whatever this system selected).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::targeting_system
