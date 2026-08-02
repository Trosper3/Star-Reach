#include "modes/space/systems/RefactorSystem.h"

#include <algorithm>
#include <vector>

#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Loot.h"
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
        CargoHold* cargo = registry.try_get<CargoHold>(self);
        const ParentRig* parent =
            registry.valid(hardpoint) ? registry.try_get<ParentRig>(hardpoint) : nullptr;
        if (cargo == nullptr || parent == nullptr || parent->root != self ||
            !DockedAtEngineeringFacility(registry, self)) {
            continue;
        }
        if (HasDependentChild(registry, self, hardpoint)) {
            continue;
        }

        const MountedModules* mounted = registry.try_get<MountedModules>(hardpoint);
        const std::size_t returning = mounted != nullptr ? mounted->ids.size() : 0;
        if (!CargoHoldHasRoomFor(*cargo, static_cast<int>(returning))) {
            continue;
        }

        if (mounted != nullptr) {
            for (const ModuleId& id : mounted->ids) {
                cargo->modules.push_back(id);
            }
        }

        Rig& rig = registry.get<Rig>(self);
        rig.children.erase(std::remove(rig.children.begin(), rig.children.end(), hardpoint),
                           rig.children.end());
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
