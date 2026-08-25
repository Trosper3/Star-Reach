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
    FacilityKind kind = FacilityKind::Trade;
    float ratePerSecond = 0.0f;  // Repair HP/s, manufacturing progress/s, research points/s.
    int capacity = 0;            // Docking bays, storage slots.
    // Meaningful only for FacilityKind::Engineering: the engineer's skill tier, scaling
    // EngineerSystem's merge formula (higher preserves more of the secondary module's stats).
    // Named "grade" rather than "level" from the start (architecture.md 13.3 finding K, re-aimed
    // by 12.19): neither name is parsed today, so authoring the eventual name costs nothing extra
    // and avoids a second rename once FacilityStats::level/Grade fold together later.
    int grade = 1;
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

// features.md section 2.7: "a module supplies a capability, an officer multiplies it, an officer
// never creates one." All four are percentage multipliers, authored directly for now -- there is
// no quality-band roll to spend a budget across yet (P11-04), so content picks a number the same
// way every other ModuleDef stat is authored. Only sensors and repair are read anywhere today
// (shared/rig/ModuleAttachment.cpp's RecomputeRigTotals, modes/space/systems/
// StationServicesSystem.cpp) -- operation and command are specified consumers (TargetingSystem's
// hardpoint-selection bias, CommanderSystem's standing orders/command authority) that land in
// their own issues; §2.4 tolerates an authored-but-not-yet-read field only because both already
// have a settled home, unlike the damage-control/navigator roles this section keeps out entirely.
struct CrewStats {
    float operation = 0.0f;  // TargetingSystem's hardpoint-selection bias; flight/gunnery boosts.
    float command = 0.0f;    // CommanderSystem's standing-order quality and command authority.
    float sensors = 0.0f;    // Multiplies the rig's aggregated SensorRange from a control shell.
    float repair = 0.0f;     // Multiplies a Repair facility's ratePerSecond from a control shell.
};

// One power level's effect on a module (features.md section 2.9, architecture.md 12.16 item 18).
// `drawMultiplier` scales the module's authored powerDraw/powerGeneration; `effectMultiplier`
// scales whatever that module's kind treats as its primary output (WeaponSystem reads it for
// Weapon-kind damage/fire rate, PhysicsSystem for Engine-kind thrust) once PowerSystem has
// confirmed the level is actually funded.
struct PowerLevelStats {
    float drawMultiplier = 1.0f;
    float effectMultiplier = 1.0f;
};

// A cheap thruster's boost is a nudge; a military one's is an afterburner -- these are per-module
// (Law 10), never global constants in PowerSystem. Normal is fixed at 1.0/1.0 by definition: it
// is the baseline every other level is authored relative to, not a per-module dial.
struct PowerLevels {
    PowerLevelStats offline{0.0f, 0.0f};
    PowerLevelStats reduced{0.6f, 0.75f};
    static constexpr PowerLevelStats normal{1.0f, 1.0f};
    PowerLevelStats boosted{1.5f, 1.3f};
};

// The authored definition of a module: the functional half of the Shell -> Component -> Module
// model (Law 4). Loaded from data/base_game/modules.json and never constructed as a C++
// literal outside registries and tests -- see Law 10 and tools/ci/check_content_pipeline.py.
struct ModuleDef {
    ModuleId id;
    std::string displayName;
    ModuleKind kind = ModuleKind::Armor;

    // Empty for unfactioned/universal content, same convention as ShipBlueprint::faction.
    // Authored per module (Law 10) once a faction wants an exclusive line; nothing gates on this
    // yet outside the Codex's display/filter (architecture.md 12.30.6), which is its only reader.
    FactionId faction;

    // features.md 2.4/12.19's eventual Grade ladder, pre-adopted here only for the Codex's own
    // display/filter tag -- every base-game module is tier 1 today (no "_ii"/"_iii" content is
    // authored yet), so this is forward-declared capacity, not a claim the ladder is wired
    // anywhere else. Named `tier` rather than `grade` to not collide with the unrelated
    // FacilityStats::grade above (engineer skill tier, Engineering-kind facilities only).
    int tier = 1;

    // The constraints puzzle (features.md section 2.2). Mass degrades handling; draw must be
    // covered by generation. These two fields are why there is no strictly-best loadout.
    float mass = 0.0f;
    float powerDraw = 0.0f;
    float powerGeneration = 0.0f;

    // Added to the hull of the shell this module occupies.
    float hullBonus = 0.0f;

    // features.md section 2.9: the draw/effect multiplier this module authors at each of the
    // four power levels. Meaningless for a module with neither powerDraw nor an effect PowerSystem
    // scales (e.g. Armor), the same "every module carries every stat block, only the relevant one
    // means anything" shape WeaponStats/ShieldStats/etc. already establish below.
    PowerLevels powerLevels;

    WeaponStats weapon;
    ShieldStats shield;
    EngineStats engine;
    FacilityStats facility;
    SensorStats sensor;
    FireControlStats fireControl;
    CargoBayStats cargoBay;
    CrewStats crew;
};

}  // namespace sr
