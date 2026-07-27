#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::party_system {

// Escort formation-keeping and shared retaliation for entt::registry-local parties
// (architecture.md section 4). Party warp is not implemented here -- there is no WarpSystem yet
// (issue #25) for a party to hand off to, so this system only covers the two behaviours that
// already have somewhere to plug in.
//
// Runs after NpcAiSystem so a member's formation steering is not immediately overwritten by
// NpcAiSystem's per-tick ThrustInput reset, and before DamageSystem so it can still see this
// tick's PendingDamage before DamageSystem drains and clears it.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::party_system
