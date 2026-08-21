#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::docking_system {

// Proximity prompts and dock/undock, ported onto the unified rig (Law 4) instead of the old
// per-craft-type Hardpoint struct.
//
//   - Prompt: every undocked, Targetable rig gets a DockPrompt naming the nearest same-faction
//     DockingBay hardpoint (on a DIFFERENT rig) within range, recomputed fresh every tick --
//     absent the instant nothing qualifies.
//   - Dock: a DockRequest naming the bay DockPrompt currently names (set by input or AI, the
//     same idiom as Combat.h's FireIntent) tags the requester Docked and removes Targetable --
//     Targeting.h already documents "a docked player is untargetable" as this system's rule to
//     enforce.
//   - Undock: an UndockRequest on a Docked rig clears Docked and restores Targetable.
//   - Immobile while docked: Velocity and ThrustInput are zeroed every tick a rig is Docked.
//     PhysicsSystem runs earlier in TickSchedule than NpcAiSystem, so whatever ThrustInput
//     NpcAiSystem writes for a still-docked rig this tick is only ever read by PhysicsSystem
//     AFTER this system has zeroed it again next tick -- no change to NpcAiSystem or
//     PhysicsSystem is needed for a docked rig to stay put.
//
// NOT a heal: docking used to regenerate hull for free (legacy StarReach2's DockRepair.cpp,
// kDockHealPerSecond) -- deleted per architecture.md 13.3 finding I / 13.4 decision 1, since
// running an unconditional free heal alongside StationServicesSystem's paid, facility-gated
// Repair made the paid path unsellable. The rate moves rather than dies: it lives on as
// FacilityStats::ratePerSecond (shared/blueprints/ModuleDef.h), read back through a docked
// station's living FacilityKind::Repair hardpoint (StationServicesSystem.cpp). One accepted
// regression: this view had no player filter, so it was NPC repair's entire mechanism -- NPCs do
// not repair until that re-homes onto ctx.economy (P4-04, architecture.md 12.30.4's dependency
// table).
//
// Eligibility is a ctx.diplomacy band lookup, not FactionRef equality (architecture.md 13.3
// finding N / features.md 5.3's six-row table): docking is refused at Distrustful and below,
// permitted at Neutral and above -- this is what makes docking at a station you do not own
// possible at all. A rig is always dockable at its own faction's bays regardless, since
// DiplomacyMatrix::Get(a, a) reads Friendly by construction. A null ctx.diplomacy fails closed.
//
// NOT implemented here: seated turrets (needs player input/camera switching -- modes/space/ui,
// issue #36) and capture, which now lives beside this file in CaptureSystem.cpp (#172) rather
// than in it -- ownership transfer of an uncrewed hull is a different proximity+hold shape than
// same-faction docking, not a variant of it.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::docking_system
