#include "modes/space/ui/RefactorMenu.h"

#include "shared/components/Rig.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::refactor_menu {

std::vector<entt::entity> DeletableHardpoints(const entt::registry& registry,
                                              entt::entity rigRoot) {
    std::vector<entt::entity> deletable;
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return deletable;
    }

    for (const entt::entity hardpoint : rig->children) {
        bool hasDependent = false;
        for (const entt::entity other : rig->children) {
            if (other == hardpoint) {
                continue;
            }
            const StructuralAttachment* attachment = registry.try_get<StructuralAttachment>(other);
            if (attachment != nullptr && attachment->attachedTo == hardpoint) {
                hasDependent = true;
                break;
            }
        }
        if (!hasDependent) {
            deletable.push_back(hardpoint);
        }
    }
    return deletable;
}

DeleteHardpointRequest BuildDeleteRequest(entt::entity hardpoint) {
    DeleteHardpointRequest request;
    request.hardpoint = hardpoint;
    return request;
}

void Draw(const Rectangle& bounds, const entt::registry& registry, entt::entity rigRoot) {
    const Rectangle content = sr::ui::DrawPanelFrame(bounds);

    const std::vector<entt::entity> deletable = DeletableHardpoints(registry, rigRoot);
    std::vector<sr::ui::Row> rows;
    rows.reserve(deletable.size());
    for (const entt::entity hardpoint : deletable) {
        (void)hardpoint;
        sr::ui::Row row;
        row.label = "hardpoint";
        rows.push_back(row);
    }
    sr::ui::DrawListView(content, rows, 0.0f, "NO DELETABLE HARDPOINTS");
}

}  // namespace sr::space::ui::refactor_menu
