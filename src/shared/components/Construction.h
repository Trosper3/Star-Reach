#pragma once

#include "shared/blueprints/Ids.h"
#include "shared/math/Vec2.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// architecture.md Law 9 names these two as its canonical Intent examples from the first draft of
// the intent-queue discipline; architecture.md 12.12 gives them a home. Set by input/UI on the
// requesting rig root; consumed and cleared by ConstructionSystem the same tick, the same idiom
// as Docking.h's DockRequest. `cost` is caller-supplied for the same reason
// architecture.md 12.7/12.10's request types are: no per-blueprint price registry exists yet.
struct BuildStationRequest {
    BlueprintId blueprint;
    Vec2 position;
    float rotation = 0.0f;
    int cost = 0;
};

struct PlaceShipRequest {
    BlueprintId blueprint;
    Vec2 position;
    float rotation = 0.0f;
    int cost = 0;
};

}  // namespace sr
