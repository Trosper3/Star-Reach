#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::targeting_system {

// Maintains every seeker's Target: which hostile rig it is engaging, and which of that rig's
// living hardpoints it is aiming at (features.md section 3.2).
//
// A seeker is any entity carrying Target, WorldTransform, FactionRef, and SensorRange, EXCEPT the
// PlayerControlled one -- features.md 3.2 forbids the player an automatic lock; they aim manually
// via AimPoint instead (WeaponSystem). Acquisition and re-acquisition both go through this system
// rather than through whatever wrote the Target, so a hardpoint dying mid-fight is noticed the
// same tick regardless of who set the target in the first place.
//
// Hostility is a ctx.diplomacy band lookup, not faction inequality (architecture.md 13.3 finding
// N): a seeker only auto-acquires a rig whose faction reads Hostile or War against its own,
// features.md 5.3's "fired on" bands. Neutral, Friendly and Allied rigs are never acquired. A null
// ctx.diplomacy fails closed -- nothing is hostile -- the same convention TemplateMarketSystem
// uses.
//
// Must run after HierarchySystem/PhysicsSystem (it reads WorldTransform) and before WeaponSystem
// (which aims at whatever this system selected).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::targeting_system
