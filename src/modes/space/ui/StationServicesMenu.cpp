#include "modes/space/ui/StationServicesMenu.h"

#include "shared/ui/HudTheme.h"

namespace sr::space::ui::station_services_menu {
namespace {
constexpr float kRowHeight = 20.0f;
}  // namespace

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

RepairRequest BuildRepairRequest(float fraction, int costForFullRepair) {
    RepairRequest request;
    request.fraction = fraction;
    request.costForFullRepair = costForFullRepair;
    return request;
}

void Draw(const Rectangle& bounds, const std::vector<ModuleId>& stationStock) {
    sr::ui::DrawBracketPanel(bounds, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f, 2.0f);

    for (std::size_t i = 0; i < stationStock.size(); ++i) {
        const int y =
            static_cast<int>(bounds.y) + static_cast<int>(kRowHeight) * static_cast<int>(i);
        DrawText(stationStock[i].str().c_str(), static_cast<int>(bounds.x) + 8, y, 12,
                 sr::ui::kLabelDim);
    }
}

}  // namespace sr::space::ui::station_services_menu
