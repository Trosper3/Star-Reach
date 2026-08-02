#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::commander_system {

// AI sub-commander standing orders, fleet dispatch, death (architecture.md 12.2, features.md
// 4.1). Spans LOD tiers, so two entry points rather than one function branching on tier
// (section 4, 11.3):
//
//   Tick        (1)    Standing orders in the active system.
//   TickCoarse  (2-3)  Abstract order resolution against core/galaxy/ records -- no steering,
//                      no projectiles.
//
// Both resolve identically today: order escalation only reads a commander's own Rig/Health, which
// is registry-local data available at either entry point, the same way DiscoverySystem's two
// entry points already share one RunTick. Neither writes ThrustInput or any other component
// NpcAiSystem/player input already exclusively owns (Physics.h's ThrustInput doc comment) --
// resolving `orders` is the mechanism; acting on it is left to whatever reads Commander::orders
// next, matching architecture.md 12.2's "build the mechanism, leave the roster policy open."
//
// The only concrete standing-order rule built here: a commander not already Retreating whose
// rig's aggregate hardpoint hull falls below a damage threshold escalates to Retreat. This is a
// placeholder threshold (kRetreatHealthFraction in the .cpp), not a tuned value -- the same
// category TemplateMarketSystem's roll weights are flagged as (architecture.md 12.7).
//
// Commander death needs no code here: destroying the rig root entity destroys its Commander
// component (and therefore its NetworkOwner-shaped `network` reference) for free -- the
// KnowledgeNetwork itself lives in core/knowledge/, outside any registry, and is untouched.
void Tick(const SystemContext& ctx);
void TickCoarse(const SystemContext& ctx);

}  // namespace sr::space::commander_system
