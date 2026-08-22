#pragma once

#include <entt/entity/entity.hpp>

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// architecture.md 12.12's RefactorMenu: delete a hardpoint from the requester's own live rig.
// Set by input/UI on the docked rig root; consumed and cleared by RefactorSystem the same tick,
// the same idiom as Docking.h's DockRequest.
struct DeleteHardpointRequest {
    entt::entity hardpoint = entt::null;
};

// architecture.md 12.30.5: rebuild, the delete inverted -- restores a mount the requester's own
// BlueprintRef authors and the live rig does not currently have (deleted, or never rebuilt after
// deletion). A MountId, not an entt::entity, because the thing being rebuilt has no entity yet --
// that is what makes it rebuildable. Set by input/UI on the docked rig root; consumed and cleared
// by RefactorSystem the same tick, the same idiom as DeleteHardpointRequest above.
struct RebuildMountRequest {
    MountId mount;
};

}  // namespace sr
