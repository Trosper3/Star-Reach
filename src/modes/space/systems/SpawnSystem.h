#pragma once

#include <optional>

#include "modes/space/systems/System.h"
#include "shared/blueprints/Ids.h"
#include "shared/math/Vec2.h"

namespace sr::space::spawn_system {

// Safe spawn placement, culling, and respawn-around-anchor (architecture.md section 4, section
// 12.36).
//
// This system never creates an entity -- Law 5 forbids modes/space/systems/ from including
// factories/, and RigFactory/NpcFactory are the only bridge from blueprint to live form. What it
// does instead operates entirely on rigs that already exist:
//
//   - Culling: a living rig with no SpawnAnchor tag of its own, farther than the cull radius from
//     every SpawnAnchor, is fully destroyed (root and hardpoints) -- there is no Tier 2/3 handoff
//     yet (architecture.md Law 2) for it to go dormant in instead.
//   - Respawn: a rig tagged RespawnPending is moved to ResolveSpawnPlacement's answer for its own
//     faction -- ported from legacy StarReach2's GetSafeSpawnPosition/GetRespawnPosition, now
//     preferring a friendly DockingBay's exit over a bare ring around the station root.
void Tick(const SystemContext& ctx);

// The second consumer SpawnSystem.h's own header comment predicted before architecture.md
// section 12.36 existed: SpaceFlight::OnEnter() has no rig yet to tag RespawnPending, so it
// resolves a placement directly rather than through the per-tick Tick() path. Exported (not
// file-local) for exactly that reason -- Tick()'s RespawnPending consumer and OnEnter() call the
// identical algorithm.
//
// Resolves where a fresh rig of `faction` should appear in this world, nearest `referencePosition`
// (a respawning rig's death position, or the system's nominal center for a first spawn):
//   1. The nearest friendly DockingBay's exit point (offset outward from the bay along the
//      station-root-to-bay vector), if it is clear.
//   2. A ring search around that same exit point, if it is contested.
//   3. A ring search around the nearest SpawnAnchor, if no friendly DockingBay exists at all.
//   4. nullopt if neither a friendly DockingBay nor any SpawnAnchor exists anywhere -- there is
//      nothing to place relative to yet. Tick()'s RespawnPending consumer leaves the request
//      pending and retries next tick (the pre-existing "no anchor yet" behavior, now also
//      covering "no bay yet"); OnEnter() has no tick to retry on and falls back to
//      `referencePosition` itself at the call site.
// `self` excludes that entity from its own clearance check (entt::null for a rig that does not
// exist yet, e.g. OnEnter's first spawn).
std::optional<Vec2> ResolveSpawnPlacement(const entt::registry& registry, const FactionId& faction,
                                          const Vec2& referencePosition,
                                          entt::entity self = entt::null,
                                          float marginUnits = 80.0f);

}  // namespace sr::space::spawn_system
