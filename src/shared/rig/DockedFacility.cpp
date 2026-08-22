#include "shared/rig/DockedFacility.h"

#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"

namespace sr::docked_facility {
namespace {

entt::entity PlayerShell(const entt::registry& registry) {
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        return entity;
    }
    return entt::null;
}

}  // namespace

entt::entity DockedFacility(const entt::registry& registry, entt::entity requester,
                            FacilityKind kind) {
    const Docked* docked = registry.try_get<Docked>(requester);
    if (docked == nullptr || !registry.valid(docked->station)) {
        return entt::null;
    }

    const entt::entity shell = PlayerShell(registry);
    if (shell == entt::null || registry.all_of<Destroyed>(shell)) {
        return entt::null;
    }

    const FacilityRef* facility = registry.try_get<FacilityRef>(shell);
    if (facility == nullptr || facility->kind != kind) {
        return entt::null;
    }

    const ParentRig* parent = registry.try_get<ParentRig>(shell);
    if (parent == nullptr || parent->root != docked->station) {
        return entt::null;
    }
    return shell;
}

}  // namespace sr::docked_facility
