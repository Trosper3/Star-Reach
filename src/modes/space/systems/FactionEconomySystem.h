#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::faction_economy_system {

// Production, stock, and spending (architecture.md section 4) -- the modes/space/ bridge onto
// core/economy/'s FactionEconomy ledger (Law 8: galaxy-wide state lives in core/, not in any one
// entt::registry, so a colonization/rebuild decision can see a faction's total stock across
// every system it controls once a galaxy actually has more than one).
//
//   - Production: a DepositRequest credits FactionEconomy::Deposit. Stands in for legacy
//     StarReach2's per-station StationEconomy production/trade -- there is no station content
//     pipeline yet (StationFactory, #41) for a real manufacturing tick to feed this instead.
//   - Spending: a SpendRequest attempts FactionEconomy::Spend and writes the outcome onto the
//     SAME entity as SpendResult for whatever requested it to read back.
//
// Both requests exist with no producer wired up yet, the same way DockRequest existed before
// player input did -- a future FactionDecisionSystem (#31) is the obvious first caller.
//
// NOT implemented here: station rebuild/upgrade. Legacy StarReach2's version reconstructs a
// destroyed station's hardpoints in place, but this codebase's DamageSystem treats destruction
// as permanent (Health.h), and rebuilding a rig from scratch is a factory operation -- Law 5
// forbids modes/space/systems/ from including factories/ at all, enforced by
// tools/ci/check_layers.py, so no system (this one included) can do it. That needs an
// orchestrator-level intent once StationFactory (#41) exists.
//
// Tier 2-3: a coarse-tier entry point is expected per the LOD contract (features.md section
// 1.1), but has no driver yet (architecture.md 13.3 finding M) -- Tick is the only entry point
// until P9-01 reinstates TickCoarse alongside the coarse loop. It would be identical to Tick
// regardless -- there is no registry-local state here to skip at the coarser tier, since the
// ledger this system reads and writes lives in core/economy/, not in this registry.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::faction_economy_system
