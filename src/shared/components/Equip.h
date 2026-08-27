#pragma once

#include <entt/entity/entity.hpp>

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// Set by input/UI on the rig root performing the equip; consumed and cleared by
// ModuleEquipSystem the same tick, the same idiom as Docking.h's DockRequest. `module` must be
// in `sourceCargo`'s CargoHold (Loot.h) and `mount` must be a hardpoint on the entity this
// request is placed on ("self"), or the request is silently dropped.
struct MountModuleRequest {
    ModuleId module;
    entt::entity mount = entt::null;
    // entt::null (the original default) means "self" -- ModulesMenu's own single-hull mount/
    // unmount never sets this. Engineering's cross-hull drag sets it to the OTHER rig's root when
    // the module being mounted came from a different hull's cargo than the one `mount` lives on,
    // so the withdrawal and the attachment can target different entities.
    entt::entity sourceCargo = entt::null;
};

// Set by input/UI; consumed and cleared by ModuleEquipSystem the same tick.
struct UnmountModuleRequest {
    entt::entity mount = entt::null;
    // entt::null (the original default) means "self" -- the refunded module returns to the same
    // rig `mount` belongs to. Engineering's cross-hull drag sets this to send the unmounted module
    // to the OTHER hull's cargo instead (a drag from a hardpoint node dropped on the other panel's
    // cargo list).
    entt::entity destinationCargo = entt::null;
};

}  // namespace sr
