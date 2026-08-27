#pragma once

#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "shared/blueprints/Ids.h"
#include "shared/math/Vec2.h"

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

// Where PopulatePrologueSystem below wants the player placed -- a wingman slot inside the
// formation it just authored, resolved before a player rig exists to place (the same split
// PopulateSystem/SpaceFlight::ResolveSpawnPlacement already follow).
struct ProloguePlacement {
    Vec2 playerPosition;
    float playerRotation = 0.0f;
};

// features.md 1.2 "Act I -- The Anomaly" / P5-06: the one hand-authored system in the game, not a
// seeded one -- every call places the identical fixed layout regardless of `world`'s own system
// id or any RNG. Spawns a same-faction NPC fleet in formation (PartyLeader/PartyMember, Party.h)
// led by a rig whose Bridge/cockpit hardpoint is directly given Commander (Commander.h) --
// deliberately NOT SeedCommanders below, which is built to promote whichever eligible officers a
// PROCEDURAL roll happens to produce and would happily tag more than one rig in a fleet this
// small; the prologue wants exactly one, chosen by this function, not by chance. Also spawns the
// anomaly itself (WorldBody + AnomalyField, shared/components/Physics.h) at a fixed offset from
// the fleet. TutorialSystem.cpp is the only reader of AnomalyField, and gates the commander's
// hail-and-offer on it existing at all -- this is therefore also the only function that can ever
// turn that offer on.
//
// Re-entrant the same way PopulateSystem is: called against an already-reset `world` (SpaceFlight
// OnEnter's own `world_ = SystemWorld(...)` before populating), so calling this twice in one
// process never produces two fleets or two anomalies.
ProloguePlacement PopulatePrologueSystem(SystemWorld& world, const core::ContentLibrary& content,
                                         const FactionId& faction);

}  // namespace sr::space::world_gen
