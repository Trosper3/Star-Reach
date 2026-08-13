#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::player_input_system {

// architecture.md 12.24 step 2 -- Law 9's queue, on the highest-frequency case in the game.
//
// Drains SetThrottleIntent and FireWeaponsIntent from ctx.intents, resolves each intent's
// ActorId against the ActorRef view, and writes ThrustInput / emplaces FireIntent on the entity
// that resolves to. An intent naming an ActorId with no live ActorRef is silently dropped --
// IntentQueue.h's own contract is that an intent can name something that died locally two ticks
// ago, and a remote actor is never a reason to assert here.
//
// Takes a bare SystemContext, same as every other system: it does not poll raylib itself
// (FlightControls, modes/space/ui/, does that) -- the headless harness's "no window,
// SystemContext-free" pattern depends on that staying true.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::player_input_system
