#pragma once

#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"

namespace sr::space::world_gen {

// Procedurally populates a freshly created SystemWorld with a sun, planets, asteroids, and an
// initial NPC presence -- the per-system-registry equivalent of legacy StarReach2's
// systems/WorldGen.cpp (architecture.md section 9's migration table).
//
// A factory, not a system (Law 5): call once, from SpaceFlight::OnEnter, before ShipFactory
// places the player rig and before any system ticks. Deterministic from `seed` -- the same seed
// always produces the same layout, so a save only needs to persist the seed rather than every
// procedurally-placed entity, and Law 2's coarse-tick warp fast-forward can rely on the same
// reproducibility OrbitBody already assumes.
//
// Every body this spawns carries WorldBody (shared/components/Physics.h), which
// WorldRenderer::DrawWorldBodies draws -- deliberately NOT CollisionRadius, which is what
// DrawShips keys off, or a sun/planet/asteroid would render as a ship.
void PopulateSystem(SystemWorld& world, const core::ContentLibrary& content, unsigned int seed);

// features.md 4.5 / architecture.md 12.2: promotes up to three already-mounted officers
// (a `CrewRating` with a non-zero `command` roll, features.md 2.7) per distinct faction present
// in `registry` to Commander -- "not a separate acquisition track," since every authored ship and
// station blueprint already mounts an officer on its Bridge/cockpit via the normal equip path
// (RigFactory). Capped at three per faction and by whatever is actually eligible; never spawns
// anything itself. Idempotent per hardpoint -- skips one that already carries Commander. Exported
// separately from PopulateSystem so it is independently unit-testable against a fixture registry
// rather than only through PopulateSystem's randomized NPC mix.
void SeedCommanders(entt::registry& registry);

}  // namespace sr::space::world_gen
