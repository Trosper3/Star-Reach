#pragma once

#include <entt/entity/entity.hpp>

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// Set by input/UI on the rig root performing the equip; consumed and cleared by
// ModuleEquipSystem the same tick, the same idiom as Docking.h's DockRequest. `module` must be
// in the requester's own CargoHold (Loot.h) and `mount` must be a hardpoint on the requester's
// own Rig, or the request is silently dropped.
struct MountModuleRequest {
    ModuleId module;
    entt::entity mount = entt::null;
};

// Set by input/UI; consumed and cleared by ModuleEquipSystem the same tick.
struct UnmountModuleRequest {
    entt::entity mount = entt::null;
};

}  // namespace sr
