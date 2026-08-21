#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::contract_system {

// Contract lifecycle: accept, tick, complete, fail (architecture.md section 4).
//
//   - Accept: an AcceptContractRequest authors a Contract directly onto the holder -- there is
//     no contract-board/offer-generation system yet for it to reference by id instead. Dropped
//     without effect if the holder already has an active Contract ("one active contract at a
//     time", ported from legacy StarReach2's locked decision).
//   - Bounty: every rig tagged Destroyed this tick (DamageSystem's generic Health/PendingDamage
//     drain, same mechanism MiningSystem reacts to) is credited toward every active Bounty
//     contract targeting its faction, exactly once ever -- tagged KillCredited so it is never
//     recounted on a later tick. Completes once killsDone reaches killsRequired.
//   - Escort: fails immediately if escortTarget is destroyed or no longer valid; otherwise
//     completes once timeRemaining counts down to zero with the target still alive. This is a
//     deliberate correction of a legacy StarReach2 bug that treated timer expiry as failure
//     for Escort too, even though its own briefing text promised survival was the win condition.
//   - Reward: rewardCredits is credited to the holder's Wallet (shared/components/Loot.h,
//     reused from LootSystem) on completion. Nothing is paid on failure.
//   - Relations: features.md 5.3 names this system as a relation writer -- in practice
//     Reputation (core/diplomacy/Reputation.h), since Contract::issuingFaction has only one named
//     party and the holder is rarely itself a faction. Completing either contract type nudges
//     the issuer's standing up; only Escort can fail today (Bounty has no timer), and failing it
//     nudges the issuer down by the same step. A no-op if issuingFaction was left default
//     (empty) or ctx.reputation is null.
//
// NOT implemented here: Courier contracts (need FactionEconomySystem's station stock, #30, which
// doesn't exist yet), and contract-board offer generation (a future UI/economy concern -- this
// system only manages a contract already accepted).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::contract_system
