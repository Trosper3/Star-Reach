#pragma once

#include <entt/entity/registry.hpp>

#include "modes/space/systems/System.h"
#include "shared/blueprints/Ids.h"

namespace sr::space::commander_system {

// AI sub-commander standing orders, threat response, retreat (architecture.md 12.2, features.md
// 4.1, P2-11). Spans LOD tiers per features.md section 1.1, but the coarse-tier entry point has
// no driver yet (architecture.md 13.3 finding M) -- Tick is the only entry point until P9-01
// reinstates TickCoarse alongside the coarse loop. Tier 1 only: cross-system dispatch,
// colonization, and recruiting a replacement commander are Tier 3 and land in P9-05/P9-03.
//
//   - Retreat escalation: a commander not already Retreating whose vessel's aggregate structural
//     integrity (shared/rig/ModuleAttachment.h) falls below a threshold escalates to Retreat.
//     Placeholder threshold (kRetreatHealthFraction in the .cpp), not a tuned value -- the same
//     category TemplateMarketSystem's roll weights are flagged as (architecture.md 12.7).
//   - Threat dispatch: a commander not Retreating that has a hostile within detection range of
//     its own vessel assigns the nearest idle (Target-less) same-faction rig near that threat to
//     engage it directly -- Commander::orders' first real reader. This is a deliberate bypass of
//     TargetingSystem's own SensorRange-gated acquisition, which is otherwise a no-op today: no
//     authored ship mounts a Sensor module (data/base_game/ships.json), so every rig's SensorRange
//     is 0 and nothing would ever organically notice a threat without this.
//   - "Order patrols" needs no code here: NpcAiSystem's own Patrol state (P2-10) already covers it
//     once nothing is dispatched against a rig.
//
// Commander death needs no code here: a hardpoint tagged Destroyed still carries its Commander
// component (Rig.h -- DamageSystem only tags, never strips), so every reader here filters on that
// tag rather than relying on the component's absence. The KnowledgeNetwork itself lives in
// core/knowledge/, outside any registry, and is untouched either way.
void Tick(const SystemContext& ctx);

// features.md 5.1's leadership pillar, Tier-1-answerable: true while `faction` has at least one
// living Commander (a Bridge/cockpit hardpoint carrying Commander and not tagged Destroyed)
// anywhere in this registry. core::ai::EvaluateSurvival's own `hasLeadership` parameter names
// this exact predicate as its intended source (architecture.md 12.2) -- no Tier-3 caller wires
// the two together yet (P9-05/P9-08), so this is the correctly-shaped, independently-tested
// bridge on its own, the same shape core/knowledge's CollapseFaction landed in before WarpSystem's
// eventual wiring (architecture.md 12.5).
bool HasLeadership(const entt::registry& registry, const FactionId& faction);

}  // namespace sr::space::commander_system
