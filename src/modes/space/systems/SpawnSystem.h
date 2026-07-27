#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::spawn_system {

// Safe spawn placement, culling, and respawn-around-anchor (architecture.md section 4).
//
// This system never creates an entity -- Law 5 forbids modes/space/systems/ from including
// factories/, and RigFactory/NpcFactory are the only bridge from blueprint to live form. What it
// does instead operates entirely on rigs that already exist:
//
//   - Culling: a living rig with no SpawnAnchor tag of its own, farther than the cull radius from
//     every SpawnAnchor, is fully destroyed (root and hardpoints) -- there is no Tier 2/3 handoff
//     yet (architecture.md Law 2) for it to go dormant in instead.
//   - Respawn-around-anchor + safe placement: a rig tagged RespawnPending is moved to the
//     nearest clear ring position around the closest SpawnAnchor -- ported from legacy
//     StarReach2's GetSafeSpawnPosition/GetRespawnPosition, replacing its fixed-radius retry loop
//     with an expanding ring search so a crowded anchor still resolves instead of falling back to
//     an unsafe spot.
//
// The ring-search placement logic is internal to this file for now (Law 11: promote to a shared
// helper only once a second caller actually needs it). A future intent-consumer spawning a
// brand-new entity -- there is no such caller yet; SpaceFlight::OnEnter does not construct
// anything for any entity type today -- would be that second consumer.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::spawn_system
