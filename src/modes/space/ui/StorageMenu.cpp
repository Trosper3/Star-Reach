#include "modes/space/ui/StorageMenu.h"

#include "shared/ui/Widgets.h"

namespace sr::space::ui::storage_menu {

std::vector<std::string> Rows(const std::vector<ItemStack>& stacks) {
    std::vector<std::string> rows;
    rows.reserve(stacks.size());
    for (const ItemStack& stack : stacks) {
        if (stack.kind == ItemKind::Module) {
            rows.push_back(stack.id);
        }
    }
    for (const ItemStack& stack : stacks) {
        if (stack.kind == ItemKind::Element) {
            rows.push_back(stack.id + " x" + std::to_string(stack.quantity));
        }
    }
    return rows;
}

void Draw(const Rectangle& bounds, const std::vector<ItemStack>& stacks) {
    const Rectangle content = sr::ui::DrawPanelFrame(bounds);

    std::vector<sr::ui::Row> rows;
    for (const std::string& label : Rows(stacks)) {
        sr::ui::Row row;
        row.label = label;
        rows.push_back(row);
    }
    sr::ui::DrawListView(content, rows, 0.0f, "NO ITEMS IN HOLD");
}

}  // namespace sr::space::ui::storage_menu
