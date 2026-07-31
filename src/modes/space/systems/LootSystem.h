#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <string>

#include "core/galaxy/WreckRecord.h"
#include "modes/space/systems/System.h"

namespace sr::space::loot_system {

// Drops, material salvage, derelict wrecks, death wrecks, and pickup radius (architecture.md
// section 4).
//
// Like SpawnSystem, this system never creates an entity from nothing -- Law 5 keeps assembly out
// of systems/, and there is no LootFactory yet. What it does with drops that already exist:
//
//   - Lifetime: a LootDrop/MaterialDrop/DeathWreck's lifetimeSeconds counts down every tick; at
//     zero it despawns unclaimed. DerelictWreck carries no lifetime and never expires on its own
//     -- it is a rare, deliberate salvage target (features.md Epic 11.1), not routine clutter.
//   - Pickup: any PlayerControlled entity whose WorldTransform comes within its own
//     CollisionRadius (plus the wreck's own radiusUnits, for DerelictWreck) of a drop collects
//     it and the drop entity is destroyed. Pickup range is deliberately never a hardcoded
//     constant -- it derives from the collector's own CollisionRadius, the same ship-scaling
//     work the architecture issue calls out.
//   - LootDrop/MaterialDrop/DeathWreck pickups land in the collector's CargoHold; DerelictWreck
//     salvage lands in its Wallet. Both are session-local components on the collector itself --
//     no core/economy/ or StorageMenu UI exists yet to hand them off to.
void Tick(const SystemContext& ctx);

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
