#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::station_services_system {

// Buy/sell modules & materials, hull repair (architecture.md 12.10), Tier 1. The ordinary-
// commerce case StationServicesMenu covers: docking anywhere, including a station the requester
// does not own -- distinct from BridgeView, which is the command surface for a station the
// player owns.
//
// Consumes every BuyItemRequest/SellItemRequest/RepairRequest (shared/components/
// StationServices.h) the same tick they're set, the same idiom DockingSystem already uses for
// DockRequest. All three require the requester to carry Docking.h's Docked -- this menu only
// applies while actually docked somewhere, and the station traded against is always
// Docked::station.
//
// Buy: refused if the requester's Wallet can't afford `cost`, or the station's own CargoHold
// does not stock `module`. Debits Wallet, moves the module from the station's CargoHold to the
// requester's.
//
// Sell: the exact inverse -- refused if the requester's CargoHold does not hold `module`.
// Credits Wallet, moves the module from the requester's CargoHold to the station's.
//
// Repair: spend = round(fraction * costForFullRepair); refused if Wallet can't afford it.
// Restores `fraction` of each hardpoint's missing hull across the requester's own Rig.
//
// MergeModuleRequest is not implemented here -- see shared/components/StationServices.h's
// comment on why.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::station_services_system
