#include "modes/space/ui/ModulesMenu.h"

#include "shared/components/Rig.h"
#include "shared/ui/Widgets.h"

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
    const Rectangle content = sr::ui::DrawPanelFrame(bounds);

    std::vector<sr::ui::Row> rows;
    for (const entt::entity mount : EquippedMounts(registry, rigRoot)) {
        sr::ui::Row row;
        row.label = registry.get<EquippedModule>(mount).id.str();
        rows.push_back(row);
    }
    sr::ui::DrawListView(content, rows, 0.0f, "NO MODULES EQUIPPED");
}

}  // namespace sr::space::ui::modules_menu
