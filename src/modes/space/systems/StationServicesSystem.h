#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::station_services_system {

// Buy/sell modules & elements, hull repair (architecture.md 12.10), Tier 1. The ordinary-
// commerce case StationServicesMenu covers: docking anywhere, including a station the requester
// does not own -- distinct from BridgeView, which is the command surface for a station the
// player owns.
//
// Consumes BuyItemRequest/SellItemRequest (shared/components/StationServices.h) the same tick
// they're set, the same idiom DockingSystem already uses for DockRequest. Both require the
// requester to carry Docking.h's Docked -- this menu only applies while actually docked
// somewhere, and the station traded against is always Docked::station.
//
// Buy: refused if the requester's Wallet can't afford `cost`, or the station's own CargoHold
// does not stock `module`. Debits Wallet, moves the module from the station's CargoHold to the
// requester's.
//
// Sell: the exact inverse -- refused if the requester's CargoHold does not hold `module`.
// Credits Wallet, moves the module from the requester's CargoHold to the station's.
//
// Repair (architecture.md 12.30.4): unlike Buy/Sell/Transfer, RepairOrder is an ORDER, not a
// request -- it persists across ticks rather than being consumed and cleared the same one. Every
// tick it exists: refused unless the docked station has a living FacilityKind::Repair hardpoint;
// heals each qualifying hardpoint of the order's subject (the requester's own vessel, or a
// station the requester owns -- "a station with a repair bay repairs itself") by up to the
// facility's authored FacilityStats::ratePerSecond * dt, never past the order's targetFraction,
// never a Destroyed hardpoint. Billed in whole credits per tick, with the fractional remainder
// carried in the order (RepairOrder::creditRemainder) so a per-tick charge never rounds to zero.
// Paid from the requester's own Wallet if it has one, else ctx.economy against its FactionRef's
// stock -- an NPC's own repair, since it carries no Wallet. The order is dropped (never resumed)
// on undock, when its subject/hardpoint is no longer valid, or once its facility is destroyed;
// it merely stalls -- billing nothing, healing nothing, staying in place -- while its payer
// cannot currently afford the tick's cost.
// Transfer (architecture.md 12.30.3's Storage half): Deposit/Withdraw, free of charge, offered
// only when the station's FactionRef equals the requester's own -- a transfer within one owner,
// never across the ownership boundary Buy/Sell cross. Refused whole, never partially applied: the
// source is withdrawn from first, and a destination with no room gets the withdrawal undone
// rather than losing the stack.
//
// Repair: refused unless the docked station has a living FacilityKind::Repair hardpoint
// (architecture.md 13.3 finding I -- deleted DockingSystem's unconditional free heal, so this is
// now the only gate). The requested `fraction` is capped by how much the facility's authored
// FacilityStats::ratePerSecond can deliver this tick; spend = round(cappedFraction *
// costForFullRepair), so a capped tick is never billed for hull it did not restore. Restores the
// (possibly capped) fraction of each living hardpoint's missing hull across the requester's own
// Rig -- a Destroyed hardpoint is never healed.
//
// MergeModuleRequest is not implemented here -- see shared/components/StationServices.h's
// comment on why.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::station_services_system
