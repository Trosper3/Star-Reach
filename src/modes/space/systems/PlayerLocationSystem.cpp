#include "modes/space/systems/PlayerLocationSystem.h"

#include "shared/components/Identity.h"
#include "shared/components/Rig.h"

namespace sr::space::player_location_system {

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    registry.clear<PlayerControlled>();

    entt::entity shell = entt::null;
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        shell = entity;
        break;  // Exactly one PlayerLocation entity (architecture.md 12.30.1).
    }
    if (shell == entt::null) {
        return;
    }

    entt::entity root = shell;
    if (const ParentRig* parent = registry.try_get<ParentRig>(shell)) {
        root = parent->root;
    }
    if (registry.valid(root)) {
        registry.emplace<PlayerControlled>(root);
    }
}

}  // namespace sr::space::player_location_system
