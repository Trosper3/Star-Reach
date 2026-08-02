#include "modes/space/ui/EngineerMenu.h"

#include "shared/ui/HudTheme.h"

namespace sr::space::ui::engineer_menu {

MergeModulesRequest BuildMergeRequest(const ModuleId& primary, const ModuleId& secondary) {
    MergeModulesRequest request;
    request.primary = primary;
    request.secondary = secondary;
    return request;
}

void Draw(const Rectangle& bounds, const ModuleId& primary, const ModuleId& secondary) {
    sr::ui::DrawBracketPanel(bounds, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f, 2.0f);

    DrawText(primary.str().c_str(), static_cast<int>(bounds.x) + 8, static_cast<int>(bounds.y) + 8,
             12, sr::ui::kLabelDim);
    DrawText(secondary.str().c_str(), static_cast<int>(bounds.x) + 8,
             static_cast<int>(bounds.y) + 28, 12, sr::ui::kLabelDim);
    DrawText("MERGE", static_cast<int>(bounds.x) + 8, static_cast<int>(bounds.y) + 48, 14,
             sr::ui::kStatusGood);
}

}  // namespace sr::space::ui::engineer_menu
