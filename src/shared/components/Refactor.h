#pragma once

#include <entt/entity/entity.hpp>

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// architecture.md 12.12's RefactorMenu: delete a hardpoint from `subject`'s live rig. Set by
// input/UI on the docked requester (the entity RefactorSystem's docked_facility::DockedFacility
// gate reads), never on `subject` itself -- consumed and cleared by RefactorSystem the same tick,
// the same idiom as Docking.h's DockRequest. `subject` is entt::null for "the requester's own
// rig" (every call site before architecture.md 12.30.5's station section); set to the docked
// station instead to edit its rig -- the same subject-vs-requester split RepairOrder already
// makes, valid only when the station's FactionRef matches the requester's own (12.30.4's "a
// station with a repair bay repairs itself," generalised to Delete/Rebuild by 12.30.5).
struct DeleteHardpointRequest {
    entt::entity hardpoint = entt::null;
    entt::entity subject = entt::null;
};

// architecture.md 12.30.5: rebuild, the delete inverted -- restores a mount `subject`'s own
// BlueprintRef authors and its live rig does not currently have (deleted, or never rebuilt after
// deletion). A MountId, not an entt::entity, because the thing being rebuilt has no entity yet --
// that is what makes it rebuildable. Set by input/UI on the docked requester; consumed and
// cleared by RefactorSystem the same tick, the same idiom as DeleteHardpointRequest above --
// `subject` follows the same entt::null-means-self, station-when-yours rule.
struct RebuildMountRequest {
    MountId mount;
    entt::entity subject = entt::null;
};

}  // namespace sr
