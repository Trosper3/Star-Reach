#include "modes/space/ui/BuildMenu.h"

#include "shared/ui/HudTheme.h"

namespace sr::space::ui::build_menu {

bool CanAfford(const Wallet& wallet, int cost) {
    return wallet.credits >= cost;
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

void Draw(const Rectangle& bounds, const Wallet& wallet, int cost) {
    sr::ui::DrawBracketPanel(bounds, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f, 2.0f);

    const Color buildColor =
        CanAfford(wallet, cost) ? sr::ui::kStatusGood : sr::ui::kStatusCritical;
    DrawText("BUILD", static_cast<int>(bounds.x) + 8, static_cast<int>(bounds.y) + 8, 14,
             buildColor);
}

}  // namespace sr::space::ui::build_menu
