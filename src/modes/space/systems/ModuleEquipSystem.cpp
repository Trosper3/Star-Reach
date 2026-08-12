#include "modes/space/systems/ModuleEquipSystem.h"

#include <algorithm>
#include <vector>

#include "shared/blueprints/Taxonomy.h"
#include "shared/components/Equip.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/rig/ModuleAttachment.h"

namespace sr::space::module_equip_system {
namespace {

bool MountBelongsToRig(const entt::registry& registry, entt::entity mount, entt::entity rigRoot) {
    const ParentRig* parent = registry.try_get<ParentRig>(mount);
    return parent != nullptr && parent->root == rigRoot;
}

void ProcessMountRequests(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> consumed;

    for (auto [self, request] : registry.view<MountModuleRequest>().each()) {
        consumed.push_back(self);

        CargoHold* cargo = registry.try_get<CargoHold>(self);
        const entt::entity mount = request.mount;
        if (cargo == nullptr || !registry.valid(mount) ||
            !MountBelongsToRig(registry, mount, self)) {
            continue;
        }
        // architecture.md 12.30.7's Destroyed sweep: a dead hardpoint cannot receive a module.
        if (registry.all_of<Destroyed>(mount)) {
            continue;
        }
        if (registry.all_of<EquippedModule>(mount)) {
            continue;  // Already occupied -- unmount first.
        }
        const ShellRole* shellRole = registry.try_get<ShellRole>(mount);
        if (shellRole == nullptr) {
            continue;
        }
        const ModuleDef* module = ctx.content.FindModule(request.module);
        if (module == nullptr || !IsMountable(module->kind, shellRole->kind)) {
            continue;
        }

        const auto held = std::find(cargo->modules.begin(), cargo->modules.end(), request.module);
        if (held == cargo->modules.end()) {
            continue;
        }

        // The mount's real authored arc (Rig.h), not a hardcoded 0.0f -- a runtime-mounted
        // weapon with a zero-width FiringArc could never satisfy AimAt's withinArc test
        // (architecture.md 13.3 finding D). A mount with no MountTraverse (should not happen for
        // a factory-built hardpoint) is treated as welded forward, the same as an authored 0.0f.
        const auto* traverse = registry.try_get<MountTraverse>(mount);
        const float traverseRadians = traverse != nullptr ? traverse->radians : 0.0f;

        rig_attachment::AttachModuleComponents(registry, mount, *module, traverseRadians);
        registry.emplace<EquippedModule>(mount, request.module);
        cargo->modules.erase(held);
    }

    for (const entt::entity self : consumed) {
        registry.remove<MountModuleRequest>(self);
    }
}

void ProcessUnmountRequests(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> consumed;

    for (auto [self, request] : registry.view<UnmountModuleRequest>().each()) {
        consumed.push_back(self);

        CargoHold* cargo = registry.try_get<CargoHold>(self);
        const entt::entity mount = request.mount;
        if (cargo == nullptr || !registry.valid(mount) ||
            !MountBelongsToRig(registry, mount, self)) {
            continue;
        }
        const EquippedModule* equipped = registry.try_get<EquippedModule>(mount);
        if (equipped == nullptr) {
            continue;
        }

        const ModuleDef* module = ctx.content.FindModule(equipped->id);
        if (module != nullptr) {
            rig_attachment::DetachModuleComponents(registry, mount, module->kind);
        }
        cargo->modules.push_back(equipped->id);
        registry.remove<EquippedModule>(mount);
    }

    for (const entt::entity self : consumed) {
        registry.remove<UnmountModuleRequest>(self);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    ProcessMountRequests(ctx);
    ProcessUnmountRequests(ctx);
}

}  // namespace sr::space::module_equip_system
