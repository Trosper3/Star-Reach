#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "shared/blueprints/Ids.h"
#include "shared/blueprints/Taxonomy.h"

namespace sr {

// A raylib-independent RGBA color -- the same "not raylib's type" reasoning as shared/math/Vec2.h:
// every component below this line must be constructible in a headless unit test and in tools/,
// and shared/ may not include raylib (architecture.md Law 8). Conversion to raylib's Color happens
// at the render boundary, in shared/ui/ or a mode's render/ directory, never here.
struct ColorRGBA {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

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

    // Independent behavior modifiers, every one optional and defaulted off -- every existing
    // authored weapon leaves all seven at their defaults and fires exactly as it always has.
    // WeaponSystem composes them rather than branching on an exclusive "weapon type": each one
    // changes what a specific other field/step means rather than adding a parallel code path, so
    // any combination (a bursting homing missile, a chargeable beam) falls out of the same tick
    // logic instead of needing its own case.

    // Continuous hitscan while FireIntent holds, instead of a discrete Projectile per shot --
    // `damage` becomes damage PER SECOND rather than per hit, and `projectileSpeed`/
    // `spreadRadians`/`projectilesPerShot` are meaningless (a beam has no travel time and no
    // pellet fan). WeaponSystem hit-tests directly and queues PendingDamage every tick it fires;
    // no Projectile entity is ever created for one.
    bool continuous = false;

    // >0 bends the fired shot toward a target rather than flying the straight line `FromAngle`
    // gives it. What it bends TOWARD depends on the other flags below (see WeaponSystem's
    // EffectiveAimPoint/homing comments): a discrete projectile curves its own velocity heading
    // after launch (ProjectileSystem); a continuous beam bends its live firing direction every
    // tick instead. Aiming BEFORE the shot is fired stays cursor-driven either way unless
    // `chargeToFire` also acquired a lock -- features.md 3.2's "no target lock" governs assisted
    // AIMING, not whether an already-fired/already-locked shot can track afterward.
    float homingTurnRatePerSecond = 0.0f;

    // Gates firing behind a hold-to-charge windup: while FireIntent holds (and, if
    // `homingTurnRatePerSecond` is also set, while a lock acquired on the press that started this
    // hold remains alive -- see WeaponSystem's lock-acquisition comment), charge accumulates
    // toward `chargeSecondsToFire`; releasing early resets it to zero, no shot fired. Once charge
    // is complete *while still held*, the weapon fires -- a discrete weapon fires once right then
    // (and keeps re-firing every `fireIntervalSeconds` for as long as the hold/lock survives,
    // driven by the ordinary cooldown gate, not by re-charging); a continuous weapon just starts
    // beaming from that tick on. There is deliberately no separate "release to fire" path: a
    // beam has no discrete instant for a release to trigger, so both behaviors share the one rule
    // "may fire once charge is full and FireIntent still holds."
    bool chargeToFire = false;
    float chargeSecondsToFire = 0.0f;

    // >1 fires this many activations spaced `burstIntervalSeconds` apart per trigger, before the
    // normal `fireIntervalSeconds` cooldown starts -- a timed sequence, distinct from
    // `projectilesPerShot`'s simultaneous fan. Once the first shot of a burst fires, the rest play
    // out on their own timer regardless of continued FireIntent/charge state (a full "mag dump"
    // per pull), stopping early only if ammo runs out mid-burst.
    int burstCount = 1;
    float burstIntervalSeconds = 0.0f;

    // -1 (default) is unlimited, matching every weapon's behavior before this field existed. A
    // non-negative value is a depleting pool: 1.0 per discrete shot (or per burst activation), or
    // 1.0 per second of continuous fire (WeaponSystem::Tick's ammo comment) -- float rather than
    // int so both drain rates share one field/one gate (`ammoRemaining <= 0.0f`) instead of a
    // per-shot-count type needing a separate continuous-rate concept. Reaching zero simply stops
    // the mount from firing again; there is no reload/resupply mechanic yet.
    float maxAmmo = -1.0f;

    // Chance [0, 1] that a fired shot flies exactly at the aim direction; the complement chance
    // it instead deviates by a random angle within +/-accuracyRadians below. Defaults to 1.0 --
    // every existing weapon fires dead-on every time, exactly as before this field existed.
    // Applies per discrete shot (each pellet of a projectilesPerShot fan rolls independently);
    // meaningless for a continuous weapon, which has no discrete shot to roll for.
    //
    // Deliberately NOT the same axis as spreadRadians: spreadRadians fans multiple SIMULTANEOUS
    // pellets across a fixed, deterministic cone (a shotgun's spread pattern, reproducible from
    // the weapon's stats alone); this is a per-shot RANDOM deviation representing shot-to-shot
    // inconsistency, not pattern shape. The roll is still a pure function of (hardpoint, pellet
    // index, tick) -- never rand()/<random> -- matching MiningSystem::RollPercent's own comment on
    // why: Law 2's coarse-tick fast-forward needs every time-dependent decision reproducible from
    // the same inputs.
    float consistency = 1.0f;

    // Maximum random angular deviation, radians either side of the aim direction, applied on the
    // (1 - consistency) fraction of shots that miss the dead-on roll. Meaningless when consistency
    // is 1.0 (the default).
    float accuracyRadians = 0.0f;

    // Overrides shared/ui/HudTheme.h's DamageTypeColor default for this weapon's projectiles/beam
    // -- a mod, or a base-game module, can give a signature look (a faction's distinctive laser
    // color, say) without inventing a new DamageType just to get a different color. Authored in
    // JSON as a "colorOverride" hex string ("RRGGBB" or "RRGGBBAA"; core/registries/
    // BlueprintJson.cpp parses it and silently leaves this nullopt on anything malformed, the same
    // "fail open on cosmetic content" idea WeaponSystem::GroupEnabled already applies to an
    // unassigned weapon group) rather than as a nested {r,g,b,a} object -- simpler to author and
    // parse than adding a fifth JsonReader::Optional overload just for this one field. Absent (not
    // a zero-initialized color) so an author who genuinely wants transparent black is never
    // confused with one who set nothing.
    std::optional<ColorRGBA> colorOverride;
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
