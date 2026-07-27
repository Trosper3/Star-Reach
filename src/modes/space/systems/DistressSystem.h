#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::distress_system {

// Distress call generation and response (architecture.md section 4).
//
//   - Generate: a DistressCallRequest raises a new DistressCall entity, unless one is already
//     active -- "one active call at a time", ported from legacy StarReach2. There is no producer
//     wired up yet; a future NpcAiSystem Flee transition (ShipUnderAttack) or a future failed-
//     warp handler (Stranded) would be the first callers, the same way DockRequest existed
//     before player input did.
//   - Respond: an AcknowledgeDistressRequest acknowledges the active call if the requester is
//     within its own SensorRange (Targeting.h) of the call's position, the same range rule
//     CommsSystem's hail already uses.
//   - Resolve: every tick, the active call's timer counts up. ShipUnderAttack fails immediately
//     if npcTarget is destroyed; otherwise both types succeed once the timer reaches
//     windowSeconds with their survival condition still met (npcTarget alive for
//     ShipUnderAttack, acknowledged for Stranded) and fail otherwise. rewardCredits lands in the
//     responder's Wallet (reused from LootSystem, #22) only on success and only if the call was
//     acknowledged.
//
// NOT implemented here: reputation rewards (need core/diplomacy/'s relation matrix, #42) and any
// UI surfacing of an active call (modes/space/ui, #35/#36).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::distress_system
