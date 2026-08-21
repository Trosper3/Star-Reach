#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::comms_system {

// Hail, dialogue lines, and the message log (architecture.md section 4).
//
//   - Message log: CommsLog lives on a singleton entity (System.h's "one legitimate cache"
//     exception), tagged CommsLogSingleton, created lazily the first time anything is logged.
//     Capped at kCommsLogCap entries, oldest dropped first -- ported verbatim from legacy
//     StarReach2's AddCommsMessage/kCommsLogCap.
//   - Hail: a HailRequest naming a target rig logs a hail line if the target is within the
//     requester's own SensorRange (Targeting.h), and a "no response" line otherwise. Both
//     outcomes consume the request the same tick.
//   - Dialogue lines: a small canned response pool, picked by a deterministic hash of
//     (requester, tick) rather than RNG state -- the same reasoning MiningSystem's element roll
//     already documents for keeping Law 2's fast-forward replayable.
//   - Relations: features.md 5.3's "successful diplomacy" trigger for this system. There is no
//     negotiation/tribute/threat intent yet, so a hail that lands (the in-range branch above)
//     is the trigger today -- it nudges the target's Reputation (core/diplomacy/Reputation.h) up
//     by a bounded step if the target carries a FactionRef. A no-op otherwise, or if
//     ctx.reputation is null.
//
// NOT implemented here: the contract-board/distress-hail flows legacy layered onto hailing
// (ContractSystem #27 and DistressSystem #28 don't exist yet) and any actual UI rendering of the
// log (modes/space/ui, #35/#36). Both read CommsLog once they exist; this system only produces
// it.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::comms_system
