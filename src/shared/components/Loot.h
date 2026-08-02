#pragma once

#include <string>
#include <vector>

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// A collectible module drop (features.md's loose battlefield loot). Blueprint-form (Law 3) --
// carries the authored ModuleId rather than a live module instance, the same reference shape a
// rig's mounts use.
struct LootDrop {
    ModuleId moduleId;
    float lifetimeSeconds = 28.0f;
};

// A collectible raw-material/salvage stack. No MaterialId registry exists yet, so this carries
// a plain string the way legacy StarReach2 did before one landed -- promote to a strong id type
// (Ids.h) in the same commit a materials content pipeline actually appears.
struct MaterialDrop {
    std::string materialId;
    int quantity = 1;
    float lifetimeSeconds = 28.0f;
};

// Left behind by a destroyed capital ship or station (features.md Epic 11.1) -- routine fighter
// kills do not drop these. Unlike LootDrop/MaterialDrop this never ages out: it is a rare,
// deliberate salvage target the player is expected to eventually reach, not battlefield clutter.
struct DerelictWreck {
    int creditsReward = 0;
    bool isCapital = false;
    // Added to the collector's own CollisionRadius (shared/components/Physics.h) for the pickup
    // check -- a wreck is physically bigger than a loose drop, sized off isCapital the same way
    // legacy StarReach2 did.
    float radiusUnits = 45.0f;
};

// One stack of a collected material, merged by id so repeated pickups of the same material do
// not grow the list.
struct MaterialStack {
    std::string materialId;
    int quantity = 0;
};

// The player's cargo, dropped as a recoverable wreck at their death location (features.md
// section 3.3 Tier 2) rather than deleted outright. Distinct from DerelictWreck: this carries
// the manifest the player actually lost and always has a recovery window. Blueprint-form
// manifest (Law 3), the same shape as CargoHold below, so it copies verbatim into
// core/galaxy/WreckRecord when its system demotes (architecture.md section 12.5) and back when
// it promotes.
struct DeathWreck {
    std::vector<ModuleId> modules;
    std::vector<MaterialStack> materials;

    // features.md section 9 leaves the recovery window's exact duration (and wall-clock vs.
    // in-game time) open; three minutes is a generous placeholder, not a tuned value.
    float lifetimeSeconds = 180.0f;
};

// Collected-but-not-yet-equipped modules and materials, in blueprint form (Law 3) -- entity
// handles never enter this, only the ids/quantities a save or a future StorageMenu UI would
// read. Not yet wired to core/economy/ or any StorageMenu (both unbuilt); this is simply where
// LootSystem puts what it collects until a consumer reads it.
struct CargoHold {
    std::vector<ModuleId> modules;
    std::vector<MaterialStack> materials;

    // Total module+material entry limit. 0 means unlimited -- every CargoHold predating this
    // field (and most test fixtures) still behaves exactly as before. RefactorSystem
    // (architecture.md 12.12) is the first real gate on it: a hardpoint deletion that would
    // push the count over capacity is refused rather than dropping modules on the floor.
    int capacity = 0;
};

// Free functions, not members -- components are plain-old-data (Law 1); behavior belongs in a
// system, even a one-line query every caller would otherwise duplicate.
inline int CargoHoldEntryCount(const CargoHold& cargo) {
    return static_cast<int>(cargo.modules.size() + cargo.materials.size());
}

inline bool CargoHoldHasRoomFor(const CargoHold& cargo, int additionalEntries) {
    return cargo.capacity <= 0 || CargoHoldEntryCount(cargo) + additionalEntries <= cargo.capacity;
}

// Salvage credits collected from DerelictWreck. Session-local wallet, not the galaxy-wide
// faction stock Law 2 reserves for core/ -- this is one collector's own total.
struct Wallet {
    int credits = 0;
};

}  // namespace sr
