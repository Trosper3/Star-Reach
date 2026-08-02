#pragma once

#include <entt/entity/entity.hpp>

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

}  // namespace sr
