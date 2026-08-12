#pragma once

#include <raylib.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <vector>

#include "shared/components/StationServices.h"

// modes/space/ui/StationServicesMenu -- architecture.md 12.10, the ordinary-commerce case:
// docking at any station, including one the player does not own. Distinct from BridgeView
// (features.md section 4's command surface for a station the player owns). modes/*/ui/ must not
// include systems/ (section 2.3); this builds BuyItemRequest/SellItemRequest/RepairRequest
// (shared/components/StationServices.h) for the caller to place on the docked requester, the
// same DockRequest idiom AvionicsMenu already uses, and never calls
// modes/space/systems/StationServicesSystem itself.
//
// Draw renders through shared/ui/Widgets.h's PanelFrame + ListView (architecture.md 12.30) --
// the shared widget layer this file's row rendering was one of the four hand-rolled clones of.
namespace sr::space::ui::station_services_menu {

// Module ids in `stationStock` the requester can afford against `walletCredits`, at a flat
// per-item `pricePerModule` -- no per-item price registry exists yet (StationServices.h's
// comment), so price is a caller-supplied constant here, not looked up. Pure -- no raylib -- so
// unit-testable.
std::vector<ModuleId> AffordableModules(const std::vector<ModuleId>& stationStock,
                                        int walletCredits, int pricePerModule);

BuyItemRequest BuildBuyRequest(const ModuleId& module, int cost);
SellItemRequest BuildSellRequest(const ModuleId& module, int value);
RepairRequest BuildRepairRequest(float fraction, int costForFullRepair);

void Draw(const Rectangle& bounds, const std::vector<ModuleId>& stationStock);

}  // namespace sr::space::ui::station_services_menu
