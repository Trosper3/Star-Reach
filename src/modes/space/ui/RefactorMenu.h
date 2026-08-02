#pragma once

#include <raylib.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <vector>

#include "shared/components/Refactor.h"

// modes/space/ui/RefactorMenu -- architecture.md 12.12: delete a hardpoint from the requester's
// own live rig. modes/*/ui/ must not include systems/ (section 2.3); this builds
// DeleteHardpointRequest (shared/components/Refactor.h) for the caller to place on the
// requester, the same DockRequest idiom AvionicsMenu already uses, and never calls
// modes/space/systems/RefactorSystem directly.
namespace sr::space::ui::refactor_menu {

// Every hardpoint on `rigRoot` no other hardpoint's StructuralAttachment points at -- deleting a
// non-leaf hardpoint would orphan its children, which RefactorSystem itself also refuses; this
// is what the menu uses to grey those out rather than let the player pick them and fail. Pure --
// no raylib -- so unit-testable.
std::vector<entt::entity> DeletableHardpoints(const entt::registry& registry, entt::entity rigRoot);

DeleteHardpointRequest BuildDeleteRequest(entt::entity hardpoint);

void Draw(const Rectangle& bounds, const entt::registry& registry, entt::entity rigRoot);

}  // namespace sr::space::ui::refactor_menu
