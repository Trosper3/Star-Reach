#pragma once

#include <string_view>
#include <vector>

#include "modes/space/systems/System.h"

namespace sr::space {

struct ScheduledSystem {
    std::string_view name;
    SystemTickFn tick;
};

// The explicit, ordered list of everything that runs in one fixed tick.
//
// This is the orchestration point Law 12 requires. Systems do not trigger each other through an
// event bus; the order below IS the causal chain, written down in one readable place. When
// damage this tick has to be visible to targeting next tick, that is a fact you can read here
// rather than reconstruct in a debugger.
//
// It is also, deliberately, a merge-conflict magnet. Two agents adding a system both touch this
// list, which forces an explicit decision about relative order instead of letting two systems
// land in whatever sequence the linker happened to pick.
//
// Ordering rules, in force regardless of what gets added:
//   1. HierarchySystem runs FIRST. Every system downstream reads WorldTransform flat, and it is
//      stale until the hierarchy pass has written it.
//   2. PowerSystem runs before anything it gates (weapons, shields, thrust).
//   3. SpawnSystem runs after HierarchySystem but before anything that reasons about distance or
//      position this tick (OrbitSystem onward), so a respawned rig's new WorldTransform is
//      settled before combat/movement systems act on it. It accepts a one-tick lag on a
//      respawned rig's OWN hardpoint transforms (HierarchySystem already ran this tick) the same
//      way PhysicsSystem/HierarchySystem's ordering already does.
//   4. PartySystem runs after NpcAiSystem (so its formation ThrustInput isn't immediately reset
//      by NpcAiSystem's per-tick clear) and before DamageSystem (so it can still read this
//      tick's PendingDamage before DamageSystem drains and clears it).
//   5. DamageSystem runs LAST among simulation systems. Destruction is the tick's final word,
//      so no system spends work on a hardpoint that is about to stop existing.
//   6. Intents are drained by the orchestrator after the whole list has run, never mid-list --
//      otherwise a system's view of this tick's input depends on its position.
const std::vector<ScheduledSystem>& TickSchedule();

// Runs TickSchedule() in order. The entire body of one fixed simulation step.
void RunTick(const SystemContext& ctx);

}  // namespace sr::space
