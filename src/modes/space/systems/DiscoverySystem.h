#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::discovery_system {

// Sensor intel, system discovery, and shared knowledge (architecture.md section 4) -- the
// modes/space/ bridge onto core/galaxy/'s DiscoveryState, the same shape FactionEconomySystem
// (#30) uses for core/economy/'s ledger.
//
// Today's rule is the simplest one that is still real: a system counts as discovered for a
// faction the instant that faction has a presence in it. The PlayerControlled entity supplies
// that presence in the active (Tier 1) registry; per features.md section 1.1's LOD model, an
// NPC party occupying a Tier 2 neighbor system will supply it too once NPC faction presence is
// worth checking there -- TickCoarse is already wired for that: it runs the identical FactionRef
// scan, just against whichever SystemWorld the coarse tick hands it.
//
// Per section 9 (legacy migration plan), GalaxyMap.cpp's 1,556 lines are mostly view/layout --
// that half belongs to modes/space/ui/ (a separate, dependent issue) once it exists. This is the
// state half: what GalaxyMap actually needed to know, not how it drew it.
void Tick(const SystemContext& ctx);
void TickCoarse(const SystemContext& ctx);

}  // namespace sr::space::discovery_system
