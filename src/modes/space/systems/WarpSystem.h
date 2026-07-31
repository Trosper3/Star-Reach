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
// System warp is NOT implemented here -- it lives in SpaceFlight.cpp's WarpToSystem (issue #96),
// not in this file, because tearing down and rebuilding a SystemWorld needs the mode-level power
// SystemContext deliberately withholds from every system (Law 6/7). A SystemWarpRequest
// (shared/components/Warp.h) is what a future producer sets; this file never reads it. Galaxy
// warp (cross-galaxy travel) is still entirely unimplemented -- it needs a system-adjacency/
// topology graph core/galaxy/ does not have yet.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::warp_system
