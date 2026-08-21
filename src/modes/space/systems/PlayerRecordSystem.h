#pragma once

#include <entt/entity/registry.hpp>

#include "shared/blueprints/Ids.h"

namespace sr::space::player_record_system {

// The player record (architecture.md 12.30.3, amending 12.30.1): a lazily-created singleton
// entity holding state that belongs to the player, not to whatever hull they occupy. Today that
// is only PlayerFaction, the same shape Comms.h's CommsLogSingleton already uses for CommsLog.
//
// No Tick() -- unlike a ticking system, nothing here runs per frame. SpaceFlight::SpawnPlayerAt
// is the only writer (at OnEnter and at every warp arrival, mirroring how Wallet carries across);
// DiscoverySystem and ConstructionSystem's requester are the readers architecture.md 12.30.3
// names by file and line.

// Creates the record on first call, updates it on every later one. `faction` replaces whatever
// was there before -- there is only ever one player, so there is nothing to merge.
void SetFaction(entt::registry& registry, const FactionId& faction);

// FactionId{} (empty) if SetFaction has never been called against this registry -- the same
// "absent, not stale" failure mode PlayerControlled's own doc comment picks for the same reason.
FactionId FactionOf(const entt::registry& registry);

}  // namespace sr::space::player_record_system
