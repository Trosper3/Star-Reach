#include "modes/space/systems/WarpSystem.h"

#include <vector>

#include "shared/components/Physics.h"
#include "shared/components/Transform.h"
#include "shared/components/Warp.h"

namespace sr::space::warp_system {

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> consumed;

    for (auto [self, request, xf] : registry.view<WarpRequest, WorldTransform>().each()) {
        xf.position = request.targetPosition;

        if (auto* prev = registry.try_get<PreviousTransform>(self)) {
            prev->position = request.targetPosition;
            prev->rotation = xf.rotation;
        }
        if (auto* velocity = registry.try_get<Velocity>(self)) {
            *velocity = Velocity{};
        }

        consumed.push_back(self);
    }

    for (const entt::entity self : consumed) {
        registry.remove<WarpRequest>(self);
    }
}

}  // namespace sr::space::warp_system
