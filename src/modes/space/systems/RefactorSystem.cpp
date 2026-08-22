#include "modes/space/systems/RefactorSystem.h"

#include <algorithm>
#include <vector>

#include "core/registries/ContentLibrary.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Refactor.h"
#include "shared/components/Rig.h"
#include "shared/rig/DockedFacility.h"
#include "shared/rig/ModuleAttachment.h"

namespace sr::space::refactor_system {
namespace {

bool HasDependentChild(const entt::registry& registry, entt::entity rigRoot,
                       entt::entity hardpoint) {
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return false;
    }
    for (const entt::entity child : rig->children) {
        if (child == hardpoint) {
            continue;
        }
        const StructuralAttachment* attachment = registry.try_get<StructuralAttachment>(child);
        if (attachment != nullptr && attachment->attachedTo == hardpoint) {
            return true;
        }
    }
    return false;
}

void ProcessDeleteRequests(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> consumed;

    for (auto [self, request] : registry.view<DeleteHardpointRequest>().each()) {
        consumed.push_back(self);

        const entt::entity hardpoint = request.hardpoint;
        Rig* rig = registry.try_get<Rig>(self);
        const ParentRig* parent =
            registry.valid(hardpoint) ? registry.try_get<ParentRig>(hardpoint) : nullptr;
        if (rig == nullptr || parent == nullptr || parent->root != self ||
            docked_facility::DockedFacility(registry, self, FacilityKind::Engineering) ==
                entt::null) {
            continue;
        }
        if (rig->children.size() <= 1) {
            continue;  // At least one hardpoint must remain (architecture.md 13.3 finding V).
        }
        if (HasDependentChild(registry, self, hardpoint)) {
            continue;
        }

        // features.md 2.2's settled reversal (architecture.md 15.2 finding 8): a hardpoint that
        // still holds modules refuses deletion instead of refunding them to cargo -- unmount
        // first, then delete. A Destroyed hardpoint is the one exception: it refunds nothing
        // either way, destroyed or not, so the modules-check does not apply to it (architecture.md
        // 12.30.5 -- losing a hardpoint in combat costs the shell, not just nothing). With no
        // refund path left, there is no cargo-room check to make at all (architecture.md 15.2
        // finding 9's count-not-mass bug was in that removed path).
        if (!registry.all_of<Destroyed>(hardpoint)) {
            const MountedModules* mounted = registry.try_get<MountedModules>(hardpoint);
            if (mounted != nullptr && !mounted->ids.empty()) {
                continue;
            }
        }

        rig->children.erase(std::remove(rig->children.begin(), rig->children.end(), hardpoint),
                            rig->children.end());
        registry.destroy(hardpoint);
    }

    for (const entt::entity self : consumed) {
        registry.remove<DeleteHardpointRequest>(self);
    }
}

// The hardpoint in `rig` carrying MountRef::id == `mount`, or entt::null -- present whether it is
// living or Destroyed. A Destroyed hardpoint is not "absent": Delete must remove it first before
// the same mount id becomes rebuildable.
entt::entity FindMount(const entt::registry& registry, const Rig& rig, const MountId& mount) {
    for (const entt::entity child : rig.children) {
        const MountRef* ref = registry.try_get<MountRef>(child);
        if (ref != nullptr && ref->id == mount) {
            return child;
        }
    }
    return entt::null;
}

void ProcessRebuildRequests(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> consumed;

    for (auto [self, request] : registry.view<RebuildMountRequest>().each()) {
        consumed.push_back(self);

        if (docked_facility::DockedFacility(registry, self, FacilityKind::Engineering) ==
            entt::null) {
            continue;
        }
        Rig* rig = registry.try_get<Rig>(self);
        const BlueprintRef* blueprintRef = registry.try_get<BlueprintRef>(self);
        if (rig == nullptr || blueprintRef == nullptr) {
            continue;
        }
        const ShipBlueprint* blueprint = ctx.content.FindShip(blueprintRef->id);
        if (blueprint == nullptr) {
            continue;
        }
        const MountBlueprint* mount = blueprint->rig.Find(request.mount);
        if (mount == nullptr) {
            continue;  // Not an authored mount -- cannot rebuild what was never there.
        }
        if (FindMount(registry, *rig, request.mount) != entt::null) {
            continue;  // Already live, or Destroyed and awaiting Delete -- not a true gap.
        }

        // Rebuild works root-outward, the opposite direction Delete's HasDependentChild works
        // leaves-inward: you cannot hang a wing off a hull that is not there.
        entt::entity parentHardpoint = entt::null;
        if (!mount->attachedTo.empty()) {
            parentHardpoint = FindMount(registry, *rig, mount->attachedTo);
            if (parentHardpoint == entt::null || registry.all_of<Destroyed>(parentHardpoint)) {
                continue;
            }
        }

        const ShellDef* shell = ctx.content.FindShell(mount->shell);
        if (shell == nullptr) {
            continue;
        }

        const entt::entity hardpoint =
            rig_attachment::CreateBareHardpoint(registry, self, *mount, *shell);
        registry.emplace<StructuralAttachment>(hardpoint, parentHardpoint);
        rig->children.push_back(hardpoint);
        // Folds the restored hardpoint's mass/propulsion/sensor contribution back into the rig's
        // aggregates -- an engine mount rebuilt bare still restores nothing without this.
        rig_attachment::RecomputeRigTotals(registry, self);
    }

    for (const entt::entity self : consumed) {
        registry.remove<RebuildMountRequest>(self);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    ProcessDeleteRequests(ctx);
    ProcessRebuildRequests(ctx);
}

}  // namespace sr::space::refactor_system
