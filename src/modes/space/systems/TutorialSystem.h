#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::tutorial_system {

// Step gating and advancement (architecture.md section 4).
//
// A Tutorial component's presence on a rig IS "tutorial active" for that rig -- there is no
// separate start/skip request; add the component to begin, remove it to abandon. Each step
// advances on its own signal, read from state other systems already produce, never mutated:
//
//   - Move: the rig has traveled kTutorialMoveDistance from its position when the tutorial
//     started (captured lazily on the first tick TutorialSystem sees it).
//   - Target: the rig has a live Target (Targeting.h).
//   - DestroyAsteroid: ANY Asteroid entity is tagged Destroyed this tick -- ported from legacy
//     StarReach2, which advanced on any asteroid destruction, not specifically the tutorial
//     rig's own kill. Must run before MiningSystem, which destroys the entity outright the same
//     tick DamageSystem tags it.
//   - CollectMaterial: the rig's CargoHold (shared/components/Loot.h) has at least one material
//     stack.
//   - Dock: the rig is tagged Docked (shared/components/Docking.h).
//
// NOT wired to an automatic signal: Sell and EquipModule (need a StorageMenu/refit UI that does
// not exist yet -- modes/space/ui, #35/#36) and Warp (legacy's step meant leaving the system;
// only local warp exists so far, #25, which is not the same action). All three remain reachable
// only by whatever future system eventually completes them -- there is no generic
// AdvanceTutorialStep entry point here for one to call yet, since nothing needs it until then.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::tutorial_system
