#include "modes/space/ui/RefactorMenu.h"

#include "shared/components/Rig.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::refactor_menu {
namespace {
constexpr float kRowHeight = 20.0f;
}  // namespace

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
    sr::ui::DrawBracketPanel(bounds, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f, 2.0f);

    const std::vector<entt::entity> deletable = DeletableHardpoints(registry, rigRoot);
    for (std::size_t i = 0; i < deletable.size(); ++i) {
        const int y =
            static_cast<int>(bounds.y) + static_cast<int>(kRowHeight) * static_cast<int>(i);
        DrawText("hardpoint", static_cast<int>(bounds.x) + 8, y, 12, sr::ui::kLabelDim);
    }
}

}  // namespace sr::space::ui::refactor_menu
