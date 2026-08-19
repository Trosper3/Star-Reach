#pragma once

#include <array>

#include "shared/blueprints/ModuleDef.h"
#include "shared/blueprints/Taxonomy.h"

namespace sr {

// On a hardpoint carrying a power cell module.
struct PowerSource {
    float generation = 0.0f;
};

// On any hardpoint that consumes power. `authoredDraw` is the module's powerDraw at Normal (the
// module's own powerDraw field, since Normal's multiplier is always 1.0 by definition);
// `levels` is the module's PowerLevels (ModuleDef.h), cached at attach time the same way
// EnginePropulsion/HardpointSensorRange already cache what a system needs without a
// ContentLibrary of its own (shared/rig/ModuleAttachment.h -- shared/ may not include core/).
// `category` is derived once from ModuleKind at attach time -- the same grouping
// ModuleAttachment's SheddingPriority already established (facilities/shields/weapons/engines,
// ascending), just named for a player-facing menu.
struct PowerLoad {
    float authoredDraw = 0.0f;
    PowerLevels levels;
    PowerCategory category = PowerCategory::Facilities;
};

// On the rig root: the player's (eventually AI's -- features.md 2.9, architecture.md 12.16 item
// 18 says the AI producer is a later issue) commanded level per category. Written directly by
// AvionicsMenu, the same request-component idiom DockRequest/UndockRequest already use: the menu
// already holds this rig's own PowerAllocation to decide which way a tap/hold toggles, so there
// is no blind-input case here for Law 9's IntentQueue to earn its indirection.
struct PowerAllocation {
    PowerLevel weapons = PowerLevel::Normal;
    PowerLevel shields = PowerLevel::Normal;
    PowerLevel engines = PowerLevel::Normal;
    PowerLevel facilities = PowerLevel::Normal;
};

// On the rig root: the player-configured order PowerSystem sheds categories in when demand still
// exceeds generation even at every category's commanded level clamped to what it can afford
// (features.md 2.9's avionics-surface menu). Index 0 sheds first. Defaults to the exact order
// PowerLoad::priority used to encode before this was configurable -- facilities, then shields,
// then weapons, then engines -- so a rig with no configured list degrades exactly as it did
// before per-category allocation existed.
struct PowerPriorityList {
    std::array<PowerCategory, 4> order = {PowerCategory::Facilities, PowerCategory::Shields,
                                          PowerCategory::Weapons, PowerCategory::Engines};
};

// On the rig root, recomputed by PowerSystem each tick from living hardpoints only.
//
// StarReach2 computed a power budget and then never read it; the throttle it produced gated
// nothing. The single consumer rule (architecture.md section 2.4) exists because of exactly
// that: if nothing reads a field here, it gets deleted rather than kept -- `weapons` (WeaponSystem
// scales fire rate) and `engines` (PhysicsSystem scales thrust) both have one; `shields`/
// `facilities` are computed the same way and wait on their own consumer the same way ModuleDef's
// per-kind stat blocks coexist unused until a module of that kind needs them.
struct PowerBudget {
    float generation = 0.0f;
    float draw = 0.0f;
    // Effective output multiplier per category -- exceeds 1.0 when that category is funded at
    // Boosted, is 0 when Offline, and clamps toward 0 under an overdraw PowerPriorityList cannot
    // fully cover (features.md 2.9, architecture.md 12.16 item 18).
    float weapons = 1.0f;
    float shields = 1.0f;
    float engines = 1.0f;
    float facilities = 1.0f;
};

// Tag written by PowerSystem onto hardpoints in a category it sheds to Offline under a severe,
// unfundable overdraw (the "generation drop" path, kept separate from ordinary allocation --
// architecture.md 12.16 item 18). Systems check for its absence rather than recomputing the
// budget themselves.
struct PowerShed {};

}  // namespace sr
