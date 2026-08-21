#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::player_location_system {

// architecture.md 12.30.1 -- PlayerLocation is the only thing gameplay code ever writes;
// PlayerControlled is derived from it here, every tick, never emplaced anywhere else.
//
// Clears PlayerControlled and re-emplaces it on the rig root PlayerLocation.shell belongs to:
// `shell` itself if it carries no ParentRig (flying, or docked at a facility whose FacilityRef
// hardpoint IS the rig root -- neither happens today, but the derivation does not assume which),
// otherwise ParentRig::root. A registry with no PlayerLocation entity (OnEnter hasn't placed a
// player yet) leaves PlayerControlled empty, matching Identity.h's documented "readers see an
// empty view rather than a stale tag" contract.
//
// Runs first in TickSchedule (modes/space/systems/SystemSchedule.cpp): every downstream reader --
// TargetingSystem's exclusion, LootSystem's collector search, CockpitHud, camera follow -- needs
// this tick's PlayerControlled settled before it acts, and the derivation depends on nothing
// upstream (Rig/ParentRig membership is fixed at spawn/attach time, not recomputed per tick the
// way HierarchySystem's WorldTransform is).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::player_location_system
