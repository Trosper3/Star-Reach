#include "modes/space/ui/EngineerMenu.h"

#include <vector>

#include "shared/ui/HudTheme.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::engineer_menu {

MergeModulesRequest BuildMergeRequest(const ModuleId& primary, const ModuleId& secondary) {
    MergeModulesRequest request;
    request.primary = primary;
    request.secondary = secondary;
    return request;
}

void Draw(const Rectangle& bounds, const ModuleId& primary, const ModuleId& secondary) {
    const Rectangle content = sr::ui::DrawPanelFrame(bounds);

    const std::vector<sr::ui::Row> rows = {
        sr::ui::Row{primary.str(), "", {}, {}, -1.0f},
        sr::ui::Row{secondary.str(), "", {}, {}, -1.0f},
    };
    sr::ui::DrawListView(content, rows, 0.0f, "NOTHING TO MERGE");

    DrawText(
        "MERGE", static_cast<int>(content.x) + 8,
        static_cast<int>(content.y + static_cast<float>(rows.size()) * sr::ui::kListRowHeight) + 8,
        14, sr::ui::kStatusGood);
}

}  // namespace sr::space::ui::engineer_menu
