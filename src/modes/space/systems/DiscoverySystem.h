#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::discovery_system {

// Sensor intel, system discovery, and shared knowledge (features.md 8.3) -- writes
// core/knowledge/'s KnowledgeNetwork, not core/galaxy/'s (now-deleted) DiscoveryState.
// features.md 8.3 settled this: "knowledge networks win... DiscoveryState cannot implement this
// section at all," since a per-viewer picture (a sub-commander three systems away seeing a threat
// the player cannot) needs a store keyed per network owner, and a faction-keyed DiscoveryState has
// no way to express two members of one faction knowing different things.
//
// Today's rule is the simplest one that is still real: a system counts as discovered for a
// faction the instant that faction has a presence in it. The player record's PlayerFaction
// (modes/space/systems/PlayerRecordSystem.h) supplies that presence in the active (Tier 1)
// registry -- not PlayerControlled/FactionRef off whatever hull the player is standing in, which
// inverts while docked at a foreign station (architecture.md 12.30.3, amending 12.30.1). Per
// features.md section 1.1's LOD model, an NPC party occupying a Tier 2 neighbor system will
// supply presence there too once NPC faction presence is worth checking there. The coarse-tier
// entry point that would run this same scan against a coarse SystemWorld has no driver yet
// (architecture.md 13.3 finding M) -- Tick is the only entry point until P9-01 reinstates
// TickCoarse alongside the coarse loop.
//
// Grants land on core::knowledge::FactionNetworkId(faction) -- the same id
// core/knowledge/KnowledgeSeeding.h registers at startup and StationFactory.cpp's placeholder
// NetworkOwner already assumes. A per-sub-commander network (Commander.h's own `network` field)
// is a separate, unclaimed writer: nothing here grants to it, since this system only reads the
// player's own faction, not any one commander's.
//
// Per section 9 (legacy migration plan), GalaxyMap.cpp's 1,556 lines are mostly view/layout --
// that half belongs to modes/space/ui/ (a separate, dependent issue) once it exists. This is the
// state half: what GalaxyMap actually needed to know, not how it drew it.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::discovery_system
