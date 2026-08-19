#pragma once

#include <string>

#include "shared/blueprints/Ids.h"
#include "shared/blueprints/Taxonomy.h"

namespace sr {

// Stat blocks. Every module carries all of them; only the one matching its kind is meaningful.
//
// The alternative -- a std::variant -- reads better in C++ and reads worse in JSON, and JSON is
// the authoring surface (Law 10). A designer adding a weapon should not have to know which arm
// of a variant they are in. Zeroed irrelevant blocks cost a few dozen bytes per *definition*,
// not per entity; definitions number in the hundreds.
struct WeaponStats {
    float damage = 0.0f;
    DamageType damageType = DamageType::Kinetic;
    float fireIntervalSeconds = 1.0f;
    float projectileSpeed = 0.0f;
    float rangeUnits = 0.0f;
    float spreadRadians = 0.0f;
    int projectilesPerShot = 1;
};

struct ShieldStats {
    float capacity = 0.0f;
    DamageType absorbs = DamageType::Kinetic;
    float rechargePerSecond = 0.0f;
    // Seconds after taking a hit before recharge resumes. Without this a shield with any
    // recharge at all is effectively unkillable under sustained low-DPS fire.
    float rechargeDelaySeconds = 0.0f;
    // Identity, never rolled (features.md section 2.7) -- how far the field reaches beyond its
    // own housing (architecture.md 12.22). Personal is what the code did before this field
    // existed: every other hardpoint on the rig was unshielded regardless of what a fighter's
    // 500-capacity generator implied to the player.
    ShieldCoverage coverage = ShieldCoverage::Personal;
    // Bubble only: hardpoints within this radius of the mount also benefit. Meaningless for
    // Personal (nothing to reach) and Conformal (rig membership decides reach, not distance).
    float coverageRadius = 0.0f;
};

struct EngineStats {
    float thrustNewtons = 0.0f;
    float turnTorque = 0.0f;
    float maxSpeed = 0.0f;
};

struct FacilityStats {
    FacilityKind kind = FacilityKind::Storage;
    float ratePerSecond = 0.0f;  // Repair HP/s, manufacturing progress/s, research points/s.
    int capacity = 0;            // Docking bays, storage slots.
    // Meaningful only for FacilityKind::Engineering: the engineer's skill tier, 1-5, scaling
    // EngineerSystem's merge formula (higher preserves more of the secondary module's stats).
    int level = 1;
};

struct SensorStats {
    // Max-aggregated onto the rig's SensorRange (architecture.md 12.23) -- two sensor arrays do
    // not see twice as far.
    float range = 0.0f;
};

struct FireControlStats {
    // Applied directly to the co-mounted Weapon's FiringArc::turnRatePerSecond, replacing the
    // un-augmented baseline (architecture.md 12.23). Per-hardpoint, not rig-aggregated -- a
    // FireControl module only ever helps the turret it shares a mount with.
    float turnRatePerSecond = 0.0f;
};

struct CargoBayStats {
    int slotCount = 0;          // How many distinct stacks this bay can hold. Variety.
    float slotCapacity = 0.0f;  // Mass ceiling per slot. Bulk. Total is derived, never authored.
};

// The authored definition of a module: the functional half of the Shell -> Component -> Module
// model (Law 4). Loaded from data/base_game/modules.json and never constructed as a C++
// literal outside registries and tests -- see Law 10 and tools/ci/check_content_pipeline.py.
struct ModuleDef {
    ModuleId id;
    std::string displayName;
    ModuleKind kind = ModuleKind::Armor;

    // The constraints puzzle (features.md section 2.2). Mass degrades handling; draw must be
    // covered by generation. These two fields are why there is no strictly-best loadout.
    float mass = 0.0f;
    float powerDraw = 0.0f;
    float powerGeneration = 0.0f;

    // Added to the hull of the shell this module occupies.
    float hullBonus = 0.0f;

    WeaponStats weapon;
    ShieldStats shield;
    EngineStats engine;
    FacilityStats facility;
    SensorStats sensor;
    FireControlStats fireControl;
    CargoBayStats cargoBay;
};

}  // namespace sr
