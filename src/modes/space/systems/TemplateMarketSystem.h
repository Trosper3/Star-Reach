#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::template_market_system {

// Template pitch, valuation, negotiation roll, royalty accrual (architecture.md 12.7,
// features.md 2.6). Tier 2-3 only -- this resolves entirely against core/ state
// (KnowledgeStore, FactionEconomy, DiplomacyMatrix, Reputation), never a registry, so unlike
// CommanderSystem/ResearchSystem it has exactly one entry point rather than a Tick/TickCoarse
// pair (section 4, 11.3): there is no Tier-1-specific behavior to split off.
//
// Consumes every PitchTemplateIntent on ctx.intents this tick (Law 9 -- the UI never resolves a
// sale directly) and resolves each in three steps, per architecture.md 12.7:
//
//   1. Gate    -- ctx.diplomacy's relation between buyerFaction and sellerFaction must be
//                 Neutral or better (Neutral, Friendly, Allied). Distrustful, Hostile and War all
//                 refuse the pitch before evaluating anything else (features.md 5.3).
//   2. Accept  -- deterministic, no roll: PitchTemplateIntent::archetypeFits and
//                 beatsCurrentManufacture must both be true, and ctx.economy must show
//                 buyerFaction can afford materialsCost. Failing any of these rejects the pitch;
//                 the Template stays in the seller's network untouched.
//   3. Roll    -- only runs if step 2 accepted. Computes a payoutMultiplier that scales
//                 basePayout; it can raise or lower the payout but never un-accept. The sale
//                 itself (ctx.knowledge->Copy) always proceeds once step 2 passes.
//
// A LumpSum payment credits sellerFaction's FactionEconomy stock once, immediately. A Royalty
// payment instead registers a stream (FactionEconomy::AddRoyalty) so every later Deposit() to
// buyerFaction skims a cut to sellerFaction automatically -- the mechanism behind "royalties
// accrue against Tier 3 production without the player present."
void Tick(const SystemContext& ctx);

}  // namespace sr::space::template_market_system
