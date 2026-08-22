#include "modes/space/ui/ModulesMenu.h"

#include <raylib.h>

#include <optional>

#include "shared/components/FlightOverlay.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::modules_menu {
namespace {

// KEY_L: mnemonic for Loadout, and free -- see StorageMenu.cpp's identical note on KEY_I; the
// same "features.md 3.6 is still 📋 and reserves neither" reasoning applies here.
constexpr int kToggleKey = KEY_L;

constexpr float kPanelWidth = 560.0f;
constexpr float kPanelTop = 80.0f;
constexpr float kHeaderHeight = 20.0f;
constexpr float kListHeight = 200.0f;
constexpr float kColumnGap = 16.0f;

const Rig* FindRig(const entt::registry& registry, entt::entity rigRoot) {
    return registry.try_get<Rig>(rigRoot);
}

// Duplicated locally per screen file (architecture.md 12.30's established rule) -- see
// StorageMenu.cpp's identical helper for the reasoning.
entt::entity EnsureSingleton(entt::registry& registry) {
    for (auto [entity] : registry.view<FlightOverlayStateSingleton>().each()) {
        return entity;
    }
    const entt::entity singleton = registry.create();
    registry.emplace<FlightOverlayStateSingleton>(singleton);
    registry.emplace<FlightOverlayState>(singleton);
    return singleton;
}

entt::entity FindSingleton(const entt::registry& registry) {
    for (auto [entity] : registry.view<FlightOverlayStateSingleton>().each()) {
        return entity;
    }
    return entt::null;
}

struct Layout {
    Rectangle header{};
    Rectangle holdList{};
    Rectangle mountList{};
};

Rectangle PanelBounds() {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    return Rectangle{(screenWidth - kPanelWidth) * 0.5f, kPanelTop, kPanelWidth,
                     kHeaderHeight + kListHeight};
}

Layout ComputeLayout() {
    const Rectangle content = sr::ui::PanelContentRect(PanelBounds());
    const float columnWidth = (content.width - kColumnGap) * 0.5f;
    Layout layout;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    const float listY = content.y + kHeaderHeight;
    layout.holdList = {content.x, listY, columnWidth, kListHeight};
    layout.mountList = {content.x + columnWidth + kColumnGap, listY, columnWidth, kListHeight};
    return layout;
}

// Distinct Module ids held anywhere in `rigRoot`'s cargo -- the loadout overlay's left `ListView`
// (architecture.md 12.30.7: "your hold, filtered to modules mountable somewhere on this rig" --
// this issue's scope stops at "held," not "mountable somewhere": ModuleEquipSystem already
// refuses an incompatible kind, and filtering ahead of it here would need ContentLibrary, which
// modes/*/ui/ has no established reason to depend on yet for this screen).
std::vector<ModuleId> HeldModules(const entt::registry& registry, entt::entity rigRoot) {
    std::vector<ModuleId> ids;
    for (const ItemStack& stack : cargo_view::Merged(registry, rigRoot)) {
        if (stack.kind == ItemKind::Module) {
            ids.push_back(ModuleId(stack.id));
        }
    }
    return ids;
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

bool IsOpen(const entt::registry& registry) {
    const entt::entity singleton = FindSingleton(registry);
    return singleton != entt::null && registry.get<FlightOverlayState>(singleton).loadoutOpen;
}

void Update(entt::registry& registry, entt::entity rigRoot) {
    if (IsKeyPressed(kToggleKey)) {
        FlightOverlayState& state = registry.get<FlightOverlayState>(EnsureSingleton(registry));
        state.loadoutOpen = !state.loadoutOpen;
    }

    if (!IsOpen(registry) || rigRoot == entt::null) {
        return;
    }

    const sr::ui::UiInput input{GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                GetMouseWheelMove()};
    if (!input.clicked) {
        return;
    }

    const Layout layout = ComputeLayout();
    FlightOverlayState& state = registry.get<FlightOverlayState>(EnsureSingleton(registry));

    const std::vector<ModuleId> held = HeldModules(registry, rigRoot);
    const std::optional<int> holdHit = sr::ui::ListViewRowAt(
        layout.holdList, static_cast<int>(held.size()), 0.0f, input.cursor);
    if (holdHit.has_value() && *holdHit < static_cast<int>(held.size())) {
        state.pendingModule = held[static_cast<std::size_t>(*holdHit)];
        return;
    }

    const std::vector<entt::entity> equipped = EquippedMounts(registry, rigRoot);
    const std::vector<entt::entity> equippable = EquippableMounts(registry, rigRoot);
    std::vector<entt::entity> mounts = equipped;
    mounts.insert(mounts.end(), equippable.begin(), equippable.end());

    const std::optional<int> mountHit = sr::ui::ListViewRowAt(
        layout.mountList, static_cast<int>(mounts.size()), 0.0f, input.cursor);
    if (!mountHit.has_value() || *mountHit >= static_cast<int>(mounts.size())) {
        return;
    }
    const entt::entity mount = mounts[static_cast<std::size_t>(*mountHit)];

    if (!IsEmpty(registry, mount)) {
        registry.emplace_or_replace<UnmountModuleRequest>(rigRoot, BuildUnmountRequest(mount));
        return;
    }
    if (!state.pendingModule.empty()) {
        registry.emplace_or_replace<MountModuleRequest>(rigRoot,
                                                        BuildMountRequest(state.pendingModule, mount));
        state.pendingModule = ModuleId();
    }
}

void Draw(const entt::registry& registry, entt::entity rigRoot) {
    if (!IsOpen(registry) || rigRoot == entt::null) {
        return;
    }

    const Layout layout = ComputeLayout();
    sr::ui::DrawPanelFrame(PanelBounds());

    const entt::entity singleton = FindSingleton(registry);
    const ModuleId pending = singleton != entt::null
                                 ? registry.get<FlightOverlayState>(singleton).pendingModule
                                 : ModuleId();

    DrawText(pending.empty() ? "HOLD -- CLICK TO SELECT" : ("SELECTED: " + pending.str()).c_str(),
            static_cast<int>(layout.header.x), static_cast<int>(layout.header.y), 14,
            sr::ui::kLabelDim);

    std::vector<sr::ui::Row> holdRows;
    for (const ModuleId& id : HeldModules(registry, rigRoot)) {
        sr::ui::Row row;
        row.label = id.str();
        row.style.disabled = id == pending;  // Already the pending selection.
        holdRows.push_back(row);
    }
    sr::ui::DrawListView(layout.holdList, holdRows, 0.0f, "HOLD EMPTY");

    std::vector<sr::ui::Row> mountRows;
    for (const entt::entity mount : EquippedMounts(registry, rigRoot)) {
        sr::ui::Row row;
        if (const MountRef* ref = registry.try_get<MountRef>(mount)) {
            row.label = ref->id.str();
        }
        const MountedModules& mounted = registry.get<MountedModules>(mount);
        row.value = mounted.ids.empty() ? "" : (mounted.ids.back().str() + "  [UNMOUNT]");
        mountRows.push_back(row);
    }
    for (const entt::entity mount : EquippableMounts(registry, rigRoot)) {
        sr::ui::Row row;
        if (const MountRef* ref = registry.try_get<MountRef>(mount)) {
            row.label = ref->id.str();
        }
        row.value = "EMPTY";
        mountRows.push_back(row);
    }
    sr::ui::DrawListView(layout.mountList, mountRows, 0.0f, "NO MOUNTS AVAILABLE");
}

}  // namespace sr::space::ui::modules_menu
