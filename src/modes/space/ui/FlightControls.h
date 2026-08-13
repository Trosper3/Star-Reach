#pragma once

#include "core/events/IntentQueue.h"
#include "shared/blueprints/Ids.h"

// modes/space/ui/FlightControls -- architecture.md 12.24 step 2 (features.md 3.6's Flight and
// Combat rows).
//
// Not engine/input/: engine/ may not include core/ (section 2.3), and a producer of core::Intent
// must. modes/*/ui/ may include core/ -- AvionicsMenu already does, and is this file's precedent
// for the whole shape: read raylib, emit an intent, never touch a component directly (Law 9).
namespace sr::space::ui::flight_controls {

// Reads this frame's raylib input and pushes at most one SetThrottleIntent -- forward (W/S),
// strafe (Q/E), and turn (A/D), every frame regardless of whether any key is held, so releasing
// one zeroes the corresponding axis next tick rather than leaving it latched -- and at most one
// FireWeaponsIntent, while the left mouse button is held.
//
// Called from SpaceFlight::Update before clock_.Advance, same as AvionicsMenu::Update: the queue
// is cleared only after the whole fixed-step loop runs, so one intent pushed here is visible to
// every fixed step this real frame, which is what makes a held key work at any frame rate.
void Poll(core::IntentQueue& out, ActorId self);

}  // namespace sr::space::ui::flight_controls
