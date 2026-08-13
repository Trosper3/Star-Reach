#pragma once

#include <cstdint>
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
// manifest (Law 3); this predates the bay model and stays its own flat shape -- it is a snapshot
// taken once at death, not a live rig with hardpoints to walk.
struct DeathWreck {
    std::vector<ModuleId> modules;
    std::vector<MaterialStack> materials;

    // features.md section 9 leaves the recovery window's exact duration (and wall-clock vs.
    // in-game time) open; three minutes is a generous placeholder, not a tuned value.
    float lifetimeSeconds = 180.0f;
};

// What kind of thing a stack holds. Unified rather than two parallel vectors (the pre-P0-10
// CargoHold's `modules`/`materials` split) so CargoView's placement/refusal logic has one code
// path, not two (Law 4).
enum class ItemKind : std::uint8_t { Module, Material };

// One stack of cargo, occupying exactly one slot in whichever CargoHold holds it.
//
// `id` is a ModuleId::str() for ItemKind::Module or a materialId for ItemKind::Material -- a
// plain string rather than a variant of the two strong id types, since every consumer (CargoView,
// StorageMenu) only ever needs to compare/display it, never resolve it back through content.
// `unitMass` is resolved by the depositing system (ContentLibrary::FindModule/FindMaterial) at
// deposit time and cached here, not looked up inside shared/rig/CargoView.h -- shared/ may not
// include core/ (architecture.md section 2.3), the same denormalize-at-the-boundary pattern
// HardpointMass/EnginePropulsion (Rig.h) already establish for rig aggregation.
//
// ItemKind::Module stacks never merge -- `quantity` is always 1, matching the pre-P0-10 behavior
// where every collected module was its own `cargo.modules` entry, and matching LootDrop's own
// one-module-per-entity shape (it has no quantity field to spill a merged stack back out through).
// Only ItemKind::Material stacks accumulate quantity, mirroring MaterialStack's existing
// merge-by-id rule.
struct ItemStack {
    ItemKind kind = ItemKind::Material;
    std::string id;
    int quantity = 0;
    float unitMass = 0.0f;
};

// One cargo bay's contents. Lives on the cargo-bay hardpoint that carries it (architecture.md
// 12.23 "The hold lives on the bay"), not on the rig root -- a rig with no CargoBay module simply
// has no CargoHold anywhere, and destroying one bay loses exactly that bay's stacks. `slotCount`
// and `slotCapacity` are copied from the mounted module's instance at attach time (the same
// pattern FacilityRef copies `kind` from ModuleDef::facility), not looked up again later.
// shared/rig/CargoView.h is the one write path -- nothing else should push_back/erase `stacks`
// directly, the same one-write-path rule DockedFacility and ModuleAttachment already follow.
struct CargoHold {
    std::vector<ItemStack> stacks;
    int slotCount = 0;
    float slotCapacity = 0.0f;  // Mass ceiling per slot, not a total -- see CargoView.h.
};

// Salvage credits collected from DerelictWreck. Session-local wallet, not the galaxy-wide
// faction stock Law 2 reserves for core/ -- this is one collector's own total.
struct Wallet {
    int credits = 0;
};

}  // namespace sr
