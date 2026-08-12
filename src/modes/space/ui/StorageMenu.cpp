#include "modes/space/ui/StorageMenu.h"

#include "shared/ui/Widgets.h"

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
    const Rectangle content = sr::ui::DrawPanelFrame(bounds);

    std::vector<sr::ui::Row> rows;
    for (const std::string& label : Rows(cargo)) {
        sr::ui::Row row;
        row.label = label;
        rows.push_back(row);
    }
    sr::ui::DrawListView(content, rows, 0.0f, "NO ITEMS IN HOLD");
}

}  // namespace sr::space::ui::storage_menu
