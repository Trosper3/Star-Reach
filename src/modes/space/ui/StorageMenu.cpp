#include "modes/space/ui/StorageMenu.h"

#include "modes/space/ui/InventoryGrid.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::storage_menu {

std::vector<std::string> Rows(const CargoHold& cargo) {
    std::vector<std::string> rows;
    rows.reserve(cargo.modules.size() + cargo.materials.size());
    for (const ModuleId& module : cargo.modules) {
        rows.push_back(module.str());
    }
    for (const MaterialStack& stack : cargo.materials) {
        rows.push_back(stack.materialId + " x" + std::to_string(stack.quantity));
    }
    return rows;
}

void Draw(const Rectangle& bounds, const CargoHold& cargo) {
    sr::ui::DrawBracketPanel(bounds, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f, 2.0f);

    const std::vector<std::string> rows = Rows(cargo);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        inventory_grid::DrawSlotRow(bounds, rows[i], static_cast<int>(i));
    }
}

}  // namespace sr::space::ui::storage_menu
