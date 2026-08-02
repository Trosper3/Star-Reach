#include <catch2/catch_test_macros.hpp>

#include "modes/space/ui/StationServicesMenu.h"

using sr::ModuleId;
namespace station_services_menu = sr::space::ui::station_services_menu;

TEST_CASE("AffordableModules returns the full stock when the price is affordable",
          "[station-services-menu]") {
    const std::vector<ModuleId> stock = {ModuleId("pulse_cannon_i"), ModuleId("deflector_i")};
    const auto affordable = station_services_menu::AffordableModules(stock, 500, 200);
    CHECK(affordable == stock);
}

TEST_CASE("AffordableModules is empty when the wallet cannot afford the flat price",
          "[station-services-menu]") {
    const std::vector<ModuleId> stock = {ModuleId("pulse_cannon_i")};
    CHECK(station_services_menu::AffordableModules(stock, 50, 200).empty());
}

TEST_CASE("BuildBuyRequest/BuildSellRequest/BuildRepairRequest carry their fields through",
          "[station-services-menu]") {
    const auto buy = station_services_menu::BuildBuyRequest(ModuleId("pulse_cannon_i"), 200);
    CHECK(buy.module == ModuleId("pulse_cannon_i"));
    CHECK(buy.cost == 200);

    const auto sell = station_services_menu::BuildSellRequest(ModuleId("pulse_cannon_i"), 150);
    CHECK(sell.module == ModuleId("pulse_cannon_i"));
    CHECK(sell.value == 150);

    const auto repair = station_services_menu::BuildRepairRequest(0.5f, 100);
    CHECK(repair.fraction == 0.5f);
    CHECK(repair.costForFullRepair == 100);
}
