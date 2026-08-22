#include "modes/space/ui/StationServicesMenu.h"

#include "shared/ui/Widgets.h"

namespace sr::space::ui::station_services_menu {

std::vector<ModuleId> AffordableModules(const std::vector<ModuleId>& stationStock,
                                        int walletCredits, int pricePerModule) {
    // Price is flat per module (no per-item price registry yet -- this header's comment), so
    // every item in stock is equally affordable or none are; there is no per-item cutoff.
    if (pricePerModule <= 0 || walletCredits < pricePerModule) {
        return {};
    }
    return stationStock;
}

BuyItemRequest BuildBuyRequest(const ModuleId& module, int cost) {
    BuyItemRequest request;
    request.module = module;
    request.cost = cost;
    return request;
}

SellItemRequest BuildSellRequest(const ModuleId& module, int value) {
    SellItemRequest request;
    request.module = module;
    request.value = value;
    return request;
}

void Draw(const Rectangle& bounds, const std::vector<ModuleId>& stationStock) {
    const Rectangle content = sr::ui::DrawPanelFrame(bounds);

    std::vector<sr::ui::Row> rows;
    rows.reserve(stationStock.size());
    for (const ModuleId& module : stationStock) {
        sr::ui::Row row;
        row.label = module.str();
        rows.push_back(row);
    }
    sr::ui::DrawListView(content, rows, 0.0f, "NO STOCK AVAILABLE");
}

}  // namespace sr::space::ui::station_services_menu
