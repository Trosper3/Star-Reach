#include "modes/space/ui/ModulesMenu.h"

#include "shared/components/Rig.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::modules_menu {
namespace {

const Rig* FindRig(const entt::registry& registry, entt::entity rigRoot) {
    return registry.try_get<Rig>(rigRoot);
}

}  // namespace

// MountedModules is the single record of a mount's contents (architecture.md 13.3 finding C,
// 13.4 decision 2): empty for both an unequipped runtime mount and a blueprint mount that never
// held anything, non-empty for both a live-refitted mount and a factory-built one. Before this,
// checking EquippedModule alone offered every blueprint-mounted hardpoint on a fresh ship as an
// empty slot -- mounting there silently destroyed the original, and scrapping duplicated it.
bool IsEmpty(const entt::registry& registry, entt::entity mount) {
    const MountedModules* mounted = registry.try_get<MountedModules>(mount);
    return mounted == nullptr || mounted->ids.empty();
}

std::vector<entt::entity> EquippableMounts(const entt::registry& registry, entt::entity rigRoot) {
    std::vector<entt::entity> mounts;
    const Rig* rig = FindRig(registry, rigRoot);
    if (rig == nullptr) {
        return mounts;
    }
    for (const entt::entity child : rig->children) {
        if (registry.all_of<ShellRole>(child) && IsEmpty(registry, child)) {
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
        if (!IsEmpty(registry, child)) {
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
        for (const ModuleId& id : registry.get<MountedModules>(mount).ids) {
            sr::ui::Row row;
            row.label = id.str();
            rows.push_back(row);
        }
    }
    sr::ui::DrawListView(content, rows, 0.0f, "NO MODULES EQUIPPED");
}

}  // namespace sr::space::ui::modules_menu
