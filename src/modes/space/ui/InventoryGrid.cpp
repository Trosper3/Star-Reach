#include "modes/space/ui/InventoryGrid.h"

#include "shared/ui/HudTheme.h"

namespace sr::space::ui::inventory_grid {
namespace {
constexpr float kRowHeight = 20.0f;
}  // namespace

void DrawSlotRow(const Rectangle& bounds, const std::string& label, int index) {
    const int y = static_cast<int>(bounds.y) + static_cast<int>(kRowHeight) * index;
    DrawText(label.c_str(), static_cast<int>(bounds.x) + 8, y, 12, sr::ui::kLabelDim);
}

}  // namespace sr::space::ui::inventory_grid
