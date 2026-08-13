#include "modes/space/systems/RefactorSystem.h"

#include <algorithm>
#include <vector>

#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Refactor.h"
#include "shared/components/Rig.h"

namespace sr::space::refactor_system {
namespace {

// Same gate EngineerSystem uses -- both menus require a living Engineering facility at the
// docked station. Duplicated locally rather than shared across the two system files: it is a
// dozen lines each, and the two systems have no other reason to depend on one another.
bool DockedAtEngineeringFacility(const entt::registry& registry, entt::entity requester) {
    const Docked* docked = registry.try_get<Docked>(requester);
    if (docked == nullptr || !registry.valid(docked->station)) {
        return false;
    }
    const Rig* stationRig = registry.try_get<Rig>(docked->station);
    if (stationRig == nullptr) {
        return false;
    }
    for (const entt::entity hardpoint : stationRig->children) {
        if (registry.all_of<Destroyed>(hardpoint)) {
            continue;
        }
        const FacilityRef* facility = registry.try_get<FacilityRef>(hardpoint);
        if (facility != nullptr && facility->kind == FacilityKind::Engineering) {
            return true;
        }
    }
    return false;
}

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
            !DockedAtEngineeringFacility(registry, self)) {
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

}  // namespace

void Tick(const SystemContext& ctx) {
    ProcessDeleteRequests(ctx);
}

}  // namespace sr::space::refactor_system
