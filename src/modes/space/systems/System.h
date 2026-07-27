#pragma once

#include "core/economy/FactionEconomy.h"
#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"

namespace sr::space {

// Everything a system is allowed to touch.
//
// This is the contract for every file under modes/space/systems/. It is deliberately narrow:
// a system gets a world, the tick's intents, read-only content, and a timestep. It does NOT get
// a pointer back to SpaceFlight, which is the single most important omission in this header.
//
// StarReach2's god object was not caused by file size -- it was caused by ownership. Because
// `SpaceFlight` held all 466 members, every new feature found its natural home already open and
// already holding everything it needed. Handing systems a mode pointer would recreate that
// pressure exactly. With this struct, reaching for mode state is not "discouraged"; there is
// nothing to reach for.
struct SystemContext {
    SystemWorld& world;
    const core::IntentQueue& intents;
    const core::ContentLibrary& content;

    // Always kFixedDeltaSeconds. Passed explicitly rather than read from a global so a system
    // can be unit-tested at any step size, and so nobody is tempted to reach for a frame delta.
    float dt;

    // Ticks elapsed in this world since it was instantiated. Systems needing wall-clock
    // behaviour derive it from this, never from GetTime() -- Law 2's fast-forward depends on
    // every time-dependent value being a pure function of tick count.
    unsigned long long tick;

    // Galaxy-wide faction stock (Law 8 -- core/economy/, not this or any registry). A pointer,
    // not a reference, and defaulted to nullptr: most systems never touch it, and every test
    // fixture predating FactionEconomySystem (#30) constructs a SystemContext without one. Only
    // FactionEconomySystem reads or writes through it today.
    core::economy::FactionEconomy* economy = nullptr;

    entt::registry& Registry() const { return world.Registry(); }
};

// The signature every system exposes.
//
// Systems are free functions in their own namespace, not classes. A class invites members, and
// members in a system are per-frame state that belongs in a component where another system can
// see it. The one legitimate exception -- expensive caches like the collision broad-phase grid
// -- lives as a component on a singleton entity, not as a private field.
//
//   namespace sr::space::physics_system {
//       void Tick(const SystemContext& ctx);
//   }
//
// A system spanning LOD tiers (features.md section 1.1) exposes a SECOND entry point rather
// than branching internally:
//
//       void TickCoarse(const SystemContext& ctx);
//
// Never one function that switches on tier. The tiers have genuinely different data available
// -- Tier 2 has no projectiles and no physics -- so one function with a branch always ends up
// reading state that does not exist at the coarser tier.
using SystemTickFn = void (*)(const SystemContext&);

}  // namespace sr::space
