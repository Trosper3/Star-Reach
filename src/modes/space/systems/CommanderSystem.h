#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::commander_system {

// AI sub-commander standing orders, fleet dispatch, death (architecture.md 12.2, features.md
// 4.1). Spans LOD tiers per features.md section 1.1, but the coarse-tier entry point has no
// driver yet (architecture.md 13.3 finding M) -- Tick is the only entry point until P9-01
// reinstates TickCoarse alongside the coarse loop.
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

}  // namespace sr::space::commander_system
