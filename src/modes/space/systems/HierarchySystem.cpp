#include "modes/space/systems/HierarchySystem.h"

#include "shared/components/Rig.h"
#include "shared/components/Transform.h"

namespace sr::space::hierarchy_system {

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    for (auto [root, rig, rootXf] : registry.view<Rig, WorldTransform>().each()) {
        for (const entt::entity child : rig.children) {
            auto* local = registry.try_get<LocalTransform>(child);
            auto* childXf = registry.try_get<WorldTransform>(child);
            if (local == nullptr || childXf == nullptr) {
                continue;  // Not every rig member is necessarily a positioned hardpoint.
            }

            if (auto* prev = registry.try_get<PreviousTransform>(child)) {
                *prev = PreviousTransform{childXf->position, childXf->rotation};
            }

            *childXf = WorldTransform{rootXf.position + Rotated(local->offset, rootXf.rotation),
                                      rootXf.rotation + local->rotation};
        }
    }
}

}  // namespace sr::space::hierarchy_system
