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
// Rendering is explicitly out of scope. WorldRenderer's only generic draw path today keys off
// {WorldTransform, PreviousTransform, CollisionRadius} and paints a ship triangle
// (modes/space/render/WorldRenderer.cpp) -- so the bodies this spawns deliberately do NOT carry
// CollisionRadius, or they would render as ships. A celestial draw path belongs to LightingPass
// or IconRenderer (issues #43/#44), not this factory.
void PopulateSystem(SystemWorld& world, const core::ContentLibrary& content, unsigned int seed);

}  // namespace sr::space::world_gen
