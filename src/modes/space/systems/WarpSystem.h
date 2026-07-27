#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::warp_system {

// Local warp, system warp, galaxy warp, and registry handoff (architecture.md section 4).
//
// Local warp is the only piece implemented here: a WarpRequest teleports a rig's
// WorldTransform/PreviousTransform to the requested position within the SAME registry and zeros
// its Velocity, so it doesn't carry pre-warp momentum into an unrelated part of the system --
// legacy StarReach2's BeginLocalWarp (its only caller was the galaxy map's in-system "warp to"
// click) charged no fuel and enforced no range limit, so neither does this.
//
// System warp and galaxy warp are NOT implemented here. Both require a "clean handoff" between
// two DIFFERENT entt::registry instances (Law 2: serialize out of A, instantiate into B, tear
// down A), which needs a container of multiple SystemWorlds addressable by system id -- there is
// no such container anywhere in this codebase yet (core/galaxy/ does not exist on disk;
// SpaceFlight holds exactly one SystemWorld as a direct member). That is a bigger prerequisite
// than Unified Serialization (#33) alone, though #33 is also needed once it exists, per
// architecture.md section 11.9's dependency note: the handoff moves a rig from live form back to
// blueprint form and forward again, which is exactly what #33's pipeline is for.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::warp_system
