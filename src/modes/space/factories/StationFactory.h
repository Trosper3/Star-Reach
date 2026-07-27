#pragma once

#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/factories/RigFactory.h"

namespace sr::space::station_factory {

// The station's entry point into Law 3's blueprint->entity pipeline -- the same kind of named
// seam NpcFactory is for the opposition (NpcFactory.h). It does not duplicate RigFactory::Spawn's
// blueprint->entity conversion, it delegates to it, so that "what makes an entity a station"
// stays out of systems/ and out of RigFactory itself. Per Law 4's unified rig, a fighter, a
// station, and a capital all come out of the same converter; each named caller of that converter
// gets to assert its own precondition on top.
//
// A station is a `mobile: false` ShipBlueprint (shared/blueprints/ShipBlueprint.h) -- there is no
// separate station blueprint type, and RigFactory already special-cases `mobile` to skip
// Propulsion/LinearDamping for one. This factory's precondition is the flip side of that: a
// `mobile: true` blueprint is a ship, not a station, and this is the one place that distinction
// is enforced rather than left to caller discipline.
using SpawnParams = rig_factory::SpawnParams;
using SpawnResult = rig_factory::SpawnResult;

// Returns an invalid result if the blueprint is unknown, fails validation, or is `mobile: true`.
SpawnResult Spawn(SystemWorld& world, const core::ContentLibrary& content,
                  const SpawnParams& params);

}  // namespace sr::space::station_factory
