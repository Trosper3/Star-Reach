// ModulesMenu.cpp's own pure data half, split out to satisfy architecture.md 2.2's 600-line file
// cap (issue #228's drag-and-drop-plus-chrome pass pushed the drawing half well past it on its
// own) -- the same split StorageScreen.cpp/StorageScreenModel.cpp already establish. Everything
// here carries no raylib and no layout, so it costs nothing to keep in its own translation unit,
// unlike Update()/Draw()'s input polling and layout math, which stay with the widgets they
// position.
#include "modes/space/ui/ModulesMenu.h"

#include <algorithm>

#include "shared/components/FlightOverlay.h"
#include "shared/components/Loot.h"
#include "shared/components/Physics.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"

namespace sr::space::ui::modules_menu {
namespace {

const Rig* FindRig(const entt::registry& registry, entt::entity rigRoot) {
    return registry.try_get<Rig>(rigRoot);
}

bool MountBelongsToRig(const entt::registry& registry, entt::entity mount, entt::entity rigRoot) {
    const ParentRig* parent = registry.try_get<ParentRig>(mount);
    return parent != nullptr && parent->root == rigRoot;
}

// Duplicated locally per screen file (architecture.md 12.30's established rule) -- see
// StorageMenu.cpp's identical helper for the reasoning. ModulesMenu.cpp keeps its own copy too
// (EnsureSingleton/Update need it, and this file must stay independently buildable).
entt::entity FindSingleton(const entt::registry& registry) {
    for (auto [entity] : registry.view<FlightOverlayStateSingleton>().each()) {
        return entity;
    }
    return entt::null;
}

// True if `moduleId` is compatible with at least one hardpoint shape on `rigRoot`, regardless of
// that hardpoint's current occupancy or Destroyed state. Answers "is this module's kind
// meaningful on this hull at all," not "is there room for it right now" -- CanMount above answers
// that, per candidate mount, at drop time.
bool MountableSomewhere(const entt::registry& registry, entt::entity rigRoot,
                        const core::ContentLibrary& content, const ModuleId& moduleId) {
    const ModuleDef* module = content.FindModule(moduleId);
    if (module == nullptr) {
        return false;
    }
    const Rig* rig = FindRig(registry, rigRoot);
    if (rig == nullptr) {
        return false;
    }
    for (const entt::entity child : rig->children) {
        if (const ShellRole* role = registry.try_get<ShellRole>(child)) {
            if (role->Accepts(module->kind)) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

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
        if (registry.all_of<ShellRole>(child) && !registry.all_of<Destroyed>(child) &&
            IsEmpty(registry, child)) {
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
        if (registry.all_of<ShellRole>(child) && !registry.all_of<Destroyed>(child) &&
            !IsEmpty(registry, child)) {
            mounts.push_back(child);
        }
    }
    return mounts;
}

std::vector<entt::entity> DestroyedMounts(const entt::registry& registry, entt::entity rigRoot) {
    std::vector<entt::entity> mounts;
    const Rig* rig = FindRig(registry, rigRoot);
    if (rig == nullptr) {
        return mounts;
    }
    for (const entt::entity child : rig->children) {
        if (registry.all_of<ShellRole>(child) && registry.all_of<Destroyed>(child)) {
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

bool CanMount(const entt::registry& registry, const core::ContentLibrary& content,
              entt::entity rigRoot, entt::entity mount, const ModuleId& module) {
    if (!registry.valid(mount) || !MountBelongsToRig(registry, mount, rigRoot)) {
        return false;
    }
    if (registry.all_of<Destroyed>(mount) || !IsEmpty(registry, mount)) {
        return false;
    }
    const ShellRole* shellRole = registry.try_get<ShellRole>(mount);
    if (shellRole == nullptr) {
        return false;
    }
    const ModuleDef* moduleDef = content.FindModule(module);
    return moduleDef != nullptr && shellRole->Accepts(moduleDef->kind);
}

bool IsOpen(const entt::registry& registry) {
    const entt::entity singleton = FindSingleton(registry);
    return singleton != entt::null && registry.get<FlightOverlayState>(singleton).loadoutOpen;
}

std::vector<ModuleId> HeldModules(const entt::registry& registry, entt::entity rigRoot,
                                  const core::ContentLibrary& content) {
    std::vector<ModuleId> ids;
    for (const ItemStack& stack : cargo_view::Merged(registry, rigRoot)) {
        if (stack.kind != ItemKind::Module) {
            continue;
        }
        const ModuleId id(stack.id);
        if (MountableSomewhere(registry, rigRoot, content, id)) {
            ids.push_back(id);
        }
    }
    return ids;
}

std::vector<entt::entity> OrderedMounts(const entt::registry& registry, entt::entity rigRoot) {
    std::vector<entt::entity> mounts = EquippedMounts(registry, rigRoot);
    const std::vector<entt::entity> equippable = EquippableMounts(registry, rigRoot);
    mounts.insert(mounts.end(), equippable.begin(), equippable.end());
    const std::vector<entt::entity> destroyed = DestroyedMounts(registry, rigRoot);
    mounts.insert(mounts.end(), destroyed.begin(), destroyed.end());
    return mounts;
}

RigTotals CurrentTotals(const entt::registry& registry, entt::entity rigRoot) {
    RigTotals totals;
    if (const BodyMass* mass = registry.try_get<BodyMass>(rigRoot)) {
        totals.mass = mass->kilograms;
    }
    if (const PowerBudget* budget = registry.try_get<PowerBudget>(rigRoot)) {
        totals.powerGeneration = budget->generation;
        totals.powerDraw = budget->draw;
    }
    return totals;
}

std::optional<RigTotals> PendingTotals(const entt::registry& registry,
                                       const core::ContentLibrary& content, entt::entity rigRoot,
                                       const FlightOverlayState& state,
                                       std::optional<entt::entity> hoveredMount,
                                       bool hoveredHoldList) {
    const RigTotals current = CurrentTotals(registry, rigRoot);

    if (!state.draggedModule.empty() && hoveredMount.has_value() &&
        CanMount(registry, content, rigRoot, *hoveredMount, state.draggedModule)) {
        const ModuleDef* module = content.FindModule(state.draggedModule);
        if (module == nullptr) {
            return std::nullopt;
        }
        RigTotals pending = current;
        pending.mass += module->mass;
        pending.powerGeneration += module->powerGeneration;
        pending.powerDraw += module->powerDraw;
        return pending;
    }

    if (state.draggedFromMount != entt::null && hoveredHoldList) {
        const MountedModules* mounted = registry.try_get<MountedModules>(state.draggedFromMount);
        if (mounted == nullptr || mounted->ids.empty()) {
            return std::nullopt;
        }
        const ModuleDef* module = content.FindModule(mounted->ids.back());
        if (module == nullptr) {
            return std::nullopt;
        }
        RigTotals pending = current;
        pending.mass = std::max(0.0f, pending.mass - module->mass);
        pending.powerGeneration = std::max(0.0f, pending.powerGeneration - module->powerGeneration);
        pending.powerDraw = std::max(0.0f, pending.powerDraw - module->powerDraw);
        return pending;
    }

    return std::nullopt;
}

}  // namespace sr::space::ui::modules_menu
