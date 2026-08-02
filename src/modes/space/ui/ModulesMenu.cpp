#include "modes/space/ui/ModulesMenu.h"

#include "modes/space/ui/InventoryGrid.h"
#include "shared/components/Rig.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::modules_menu {
namespace {

const Rig* FindRig(const entt::registry& registry, entt::entity rigRoot) {
    return registry.try_get<Rig>(rigRoot);
}

}  // namespace

std::vector<entt::entity> EquippableMounts(const entt::registry& registry, entt::entity rigRoot) {
    std::vector<entt::entity> mounts;
    const Rig* rig = FindRig(registry, rigRoot);
    if (rig == nullptr) {
        return mounts;
    }
    for (const entt::entity child : rig->children) {
        if (registry.all_of<ShellRole>(child) && !registry.all_of<EquippedModule>(child)) {
            mounts.push_back(child);
        }
    }
    return mounts;
}

std::vector<entt::entity> EquippedMounts(const entt::registry& registry, entt::entity rigRoot) {
    std::vector<entt::entity> mounts;
    const Rig* rig = FindRig(registry, rigRoot);
    if (rig == nullptr) {
        return mounts;
    }
    for (const entt::entity child : rig->children) {
        if (registry.all_of<EquippedModule>(child)) {
            mounts.push_back(child);
        }
    }
    return mounts;
}

MountModuleRequest BuildMountRequest(const ModuleId& module, entt::entity mount) {
    MountModuleRequest request;
    request.module = module;
    request.mount = mount;
    return request;
}

UnmountModuleRequest BuildUnmountRequest(entt::entity mount) {
    UnmountModuleRequest request;
    request.mount = mount;
    return request;
}

void Draw(const Rectangle& bounds, const entt::registry& registry, entt::entity rigRoot) {
    sr::ui::DrawBracketPanel(bounds, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f, 2.0f);

    const std::vector<entt::entity> equipped = EquippedMounts(registry, rigRoot);
    for (std::size_t i = 0; i < equipped.size(); ++i) {
        const ModuleId& id = registry.get<EquippedModule>(equipped[i]).id;
        inventory_grid::DrawSlotRow(bounds, id.str(), static_cast<int>(i));
    }
}

}  // namespace sr::space::ui::modules_menu
