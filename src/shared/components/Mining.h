#pragma once

#include <string>
#include <vector>

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// Tag: this entity is an asteroid. DamageSystem's Health/PendingDamage drain (DamageSystem.cpp)
// is generic and tags a depleted hull Destroyed regardless of what kind of entity carries it --
// this is the only thing that tells MiningSystem an asteroid's Destroyed tag apart from a rig
// hardpoint's, so it does not roll salvage for a turret that just died.
struct Asteroid {};

// One material's independent chance to appear as salvage when the asteroid carrying it depletes.
// Percent is a standalone roll per entry, not a distribution across entries -- a rich asteroid
// can plausibly yield two or three different materials from one kill, same as legacy
// StarReach2's MaterialChance.
struct MaterialChance {
    std::string materialId;
    int percent = 0;  // 1-100
};

// Rolled once at spawn time (WorldGen, not yet built -- issue #40) and consumed exactly once,
// the tick MiningSystem sees this entity tagged Destroyed.
struct AsteroidComposition {
    std::vector<MaterialChance> materials;
};

}  // namespace sr
