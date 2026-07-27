#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::docking_system {

// Proximity prompts, dock/undock, and the heal-while-docked behavior legacy StarReach2's
// DockRepair.cpp built for NPC retreat (architecture.md section 9's migration plan), ported onto
// the unified rig (Law 4) instead of the old per-craft-type Hardpoint struct.
//
//   - Prompt: every undocked, Targetable rig gets a DockPrompt naming the nearest same-faction
//     DockingBay hardpoint (on a DIFFERENT rig) within range, recomputed fresh every tick --
//     absent the instant nothing qualifies.
//   - Dock: a DockRequest naming the bay DockPrompt currently names (set by input or AI, the
//     same idiom as Combat.h's FireIntent) tags the requester Docked and removes Targetable --
//     Targeting.h already documents "a docked player is untargetable" as this system's rule to
//     enforce.
//   - Heal: every tick a rig is Docked, its living hardpoints regenerate hull at
//     kDockHealPerSecond of their own max -- the same formula legacy DockRepair.cpp used.
//   - Undock: an UndockRequest on a Docked rig clears Docked and restores Targetable.
//   - Immobile while docked: Velocity and ThrustInput are zeroed every tick a rig is Docked.
//     PhysicsSystem runs earlier in TickSchedule than NpcAiSystem, so whatever ThrustInput
//     NpcAiSystem writes for a still-docked rig this tick is only ever read by PhysicsSystem
//     AFTER this system has zeroed it again next tick -- no change to NpcAiSystem or
//     PhysicsSystem is needed for a docked rig to stay put.
//
// Faction eligibility is simplified to "same FactionRef" rather than legacy's non-hostile
// relation check -- core/diplomacy/ (issue #42) does not exist yet for a real relation lookup.
//
// NOT implemented here: seated turrets (needs player input/camera switching -- modes/space/ui,
// issue #36) and capture (needs core/diplomacy/'s relation matrix, issue #42, plus a "disabled"
// concept -- alive but with no living Weapon hardpoint -- that does not exist anywhere yet).
// Left for those issues, the same way PartySystem left party warp for WarpSystem (#25).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::docking_system
