#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::loot_system {

// Drops, material salvage, derelict wrecks, and pickup radius (architecture.md section 4).
//
// Like SpawnSystem, this system never creates an entity -- Law 5 keeps assembly out of
// systems/, and there is no LootFactory yet. What it does with drops that already exist:
//
//   - Lifetime: a LootDrop/MaterialDrop's lifetimeSeconds counts down every tick; at zero it
//     despawns unclaimed. DerelictWreck carries no lifetime and never expires on its own -- it
//     is a rare, deliberate salvage target (features.md Epic 11.1), not routine clutter.
//   - Pickup: any PlayerControlled entity whose WorldTransform comes within its own
//     CollisionRadius (plus the wreck's own radiusUnits, for DerelictWreck) of a drop collects
//     it and the drop entity is destroyed. Pickup range is deliberately never a hardcoded
//     constant -- it derives from the collector's own CollisionRadius, the same ship-scaling
//     work the architecture issue calls out.
//   - LootDrop/MaterialDrop pickups land in the collector's CargoHold; DerelictWreck salvage
//     lands in its Wallet. Both are session-local components on the collector itself -- no
//     core/economy/ or StorageMenu UI exists yet to hand them off to.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::loot_system
