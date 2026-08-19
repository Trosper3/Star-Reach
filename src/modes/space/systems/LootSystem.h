#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <string>

#include "core/galaxy/WreckRecord.h"
#include "modes/space/systems/System.h"

namespace sr::space::loot_system {

// Drops, element salvage, derelict wrecks, death wrecks, pickup radius, and (since P0-10) the
// producer half of the cargo-bay model (architecture.md section 4, architecture.md 12.23).
//
// Like SpawnSystem, this system never assembles a *rig* from nothing -- Law 5 keeps blueprint
// assembly out of systems/. A bare drop entity (LootDrop/ElementDrop) is not a rig -- one
// transform, one drop component, no hardpoints -- so spawning one here is not the RigFactory-only
// path Law 5 protects; MiningSystem already sets this precedent for ElementDrop.
//
//   - Lifetime: a LootDrop/ElementDrop/DeathWreck's lifetimeSeconds counts down every tick; at
//     zero it despawns unclaimed. DerelictWreck carries no lifetime and never expires on its own
//     -- it is a rare, deliberate salvage target (features.md Epic 11.1), not routine clutter.
//   - Pickup: any PlayerControlled entity whose WorldTransform comes within its own
//     CollisionRadius (plus the wreck's own radiusUnits, for DerelictWreck) of a drop collects
//     it and the drop entity is destroyed, UNLESS its CargoHold(s) have no room (CargoView::
//     Deposit refuses) -- in which case the drop is left alone and keeps counting down its own
//     lifetime, the same as if no collector were in range at all. Pickup range is deliberately
//     never a hardcoded constant -- it derives from the collector's own CollisionRadius, the
//     same ship-scaling work the architecture issue calls out.
//   - LootDrop/ElementDrop/DeathWreck pickups go through shared/rig/CargoView.h's Deposit, onto
//     whichever of the collector's living cargo bays has room; DerelictWreck salvage lands in its
//     Wallet. A collector with no CargoBay hardpoint collects no items (it still has no CargoHold
//     anywhere to deposit into) but still banks DerelictWreck credits.
//   - The producer half: a cargo-bay hardpoint tagged Destroyed spills exactly its own stacks as
//     recoverable LootDrop/ElementDrop entities and empties, rather than its contents simply
//     vanishing with it (architecture.md 12.23's "shoot the bay, lose what was in it" -- as
//     recoverable salvage, not as nothing). ModuleEquipSystem's unmount path calls
//     SpillCargoHold directly for the same reason before it detaches a loaded bay.
//   - Player death (architecture.md 13.3 R, features.md section 3.3): a PlayerControlled rig with
//     every hardpoint destroyed has every mounted module and every cargo-bay stack folded into one
//     DeathWreck at the death position, then its hardpoints are stripped and revived in place and
//     RespawnPending is handed to SpawnSystem to carry the bare hull home to the nearest
//     SpawnAnchor. This is the only producer RespawnPending has.
void Tick(const SystemContext& ctx);

// Spawns a LootDrop (ItemKind::Module) or ElementDrop (ItemKind::Element) at `hardpoint`'s
// WorldTransform for every stack currently in its CargoHold, then empties it. A no-op if
// `hardpoint` carries no CargoHold or an empty one. Exposed (not file-local) because
// ModuleEquipSystem's unmount path needs the identical spill behavior Tick's own Destroyed sweep
// uses -- "unmounting a loaded bay takes the same path as destroying one" (architecture.md 12.23).
void SpillCargoHold(entt::registry& registry, entt::entity hardpoint);

// The demotion/promotion path for a DeathWreck (architecture.md section 12.5) -- the same
// blueprint/live-form bridge Law 3 requires everywhere else, and the first concrete instance of
// it in this codebase. Collapse writes a durable core::galaxy::WreckRecord from a live entity;
// Promote does the reverse. Neither is on ctx's per-tick clock -- each is a one-shot conversion,
// called around whatever tears down or instantiates the entity's SystemWorld.

// `entity` must carry DeathWreck and WorldTransform. Destroys `entity`.
core::galaxy::WreckRecord CollapseDeathWreck(entt::registry& registry, entt::entity entity,
                                             const std::string& systemId);

// Re-instantiates a DeathWreck entity from `record` into `registry`, contents intact.
entt::entity PromoteDeathWreck(entt::registry& registry, const core::galaxy::WreckRecord& record);

}  // namespace sr::space::loot_system
