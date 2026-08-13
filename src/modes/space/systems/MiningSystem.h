#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::mining_system {

// Asteroid depletion and station mining ticks (architecture.md section 4).
//
// Depletion reuses DamageSystem's Health/PendingDamage drain unchanged -- that loop
// (DamageSystem.cpp) is generic over any entity carrying both components, not just rig
// hardpoints, and its Rig-specific third loop never touches an Asteroid because asteroids do
// not carry one. This system's own job starts where DamageSystem's ends: an Asteroid tagged
// Destroyed this tick rolls its AsteroidComposition for element salvage -- one ElementDrop
// entity per element that hits (shared/components/Loot.h, the same component LootSystem
// already collects) -- then the asteroid entity itself is destroyed. Spawning that ElementDrop
// directly, the same way WeaponSystem spawns a Projectile directly, is not a Law 5 violation:
// Law 5 reserves factories for composite/blueprint objects, and a single-component drop is
// neither.
//
// Station mining ticks (auto-collecting an element into a station's storage on a timer) are NOT
// implemented here. Legacy StarReach2's version reads a "mining_station" facility def and an
// "aux_material_probe" module that have no equivalent in data/base_game/ yet, and there is no
// StationFactory (issue #41) to instantiate one against for a test to exercise against. Wiring
// it up belongs with #41, the same way PartySystem left party warp for WarpSystem (#25).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::mining_system
