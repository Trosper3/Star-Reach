#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>

#include "shared/blueprints/Taxonomy.h"

// shared/rig/ -- beside ModuleAttachment.h and CargoView.h, the same "one write path" promotion
// architecture.md 12.30.5 gives this lookup. EngineerSystem and RefactorSystem each carried their
// own copy of "is the requester docked at a station with a living facility of this kind" (a dozen
// lines apiece, justified at the time as "the two systems have no other reason to depend on one
// another"). With four verbs across two systems behind one gate, that justification runs out
// (Law 11's tie-breaker) -- one function here, which both may include since modes/space/systems/
// may not include modes/space/factories/ but may include shared/ (section 2.3).
namespace sr::docked_facility {

// The living `kind` facility hardpoint `requester` is currently standing in, or entt::null.
//
// Reads PlayerLocation directly rather than re-scanning the docked station's Rig::children for
// the first hardpoint of `kind` -- architecture.md 12.30.5: "duplicate facilities are not
// fungible" once a kind's grade decides the outcome of the operation, so which of two benches you
// get must be the one you are standing in, not iteration order. This is also what "the bench you
// selected is the bench that is used" means operationally: BridgeView::SelectTab already wrote
// PlayerLocation onto the specific hardpoint when the player picked this screen's tab (or its own
// sibling selector), so reading it back here is the whole fix.
//
// Requires `requester` to be Docked, and the PlayerLocation hardpoint's ParentRig to name that
// same station -- a player standing on one station's Engineering bench cannot spend a different
// station's grade.
entt::entity DockedFacility(const entt::registry& registry, entt::entity requester,
                            FacilityKind kind);

}  // namespace sr::docked_facility
