#include "modes/space/ui/BuildMenu.h"

#include <algorithm>

#include "shared/ui/HudTheme.h"

namespace sr::space::ui::build_menu {

bool CanAfford(const Wallet& wallet, int cost, const CargoHold& cargo,
               const std::vector<ItemStack>& materialCost) {
    if (wallet.credits < cost) {
        return false;
    }
    for (const ItemStack& required : materialCost) {
        const auto held = std::find_if(
            cargo.stacks.begin(), cargo.stacks.end(), [&required](const ItemStack& stack) {
                return stack.kind == required.kind && stack.id == required.id;
            });
        if (held == cargo.stacks.end() || held->quantity < required.quantity) {
            return false;
        }
    }
    return true;
}

BuildStationRequest BuildStationBuildRequest(const BlueprintId& blueprint, Vec2 position,
                                             float rotation, int cost) {
    BuildStationRequest request;
    request.blueprint = blueprint;
    request.position = position;
    request.rotation = rotation;
    request.cost = cost;
    return request;
}

PlaceShipRequest BuildPlaceShipRequest(const BlueprintId& blueprint, Vec2 position, float rotation,
                                       int cost) {
    PlaceShipRequest request;
    request.blueprint = blueprint;
    request.position = position;
    request.rotation = rotation;
    request.cost = cost;
    return request;
}

void Draw(const Rectangle& bounds, const Wallet& wallet, int cost, const CargoHold& cargo,
          const std::vector<ItemStack>& materialCost) {
    sr::ui::DrawBracketPanel(bounds, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f, 2.0f);

    const Color buildColor = CanAfford(wallet, cost, cargo, materialCost) ? sr::ui::kStatusGood
                                                                          : sr::ui::kStatusCritical;
    DrawText("BUILD", static_cast<int>(bounds.x) + 8, static_cast<int>(bounds.y) + 8, 14,
             buildColor);
}

}  // namespace sr::space::ui::build_menu
