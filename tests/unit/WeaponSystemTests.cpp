#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/factories/RigFactory.h"
#include "modes/space/systems/WeaponSystem.h"
#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Health.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"

namespace rig_factory = sr::space::rig_factory;

using Catch::Approx;
using sr::AimPoint;
using sr::DamageType;
using sr::Destroyed;
using sr::Docked;
using sr::EnabledWeaponGroups;
using sr::FireIntent;
using sr::FiringArc;
using sr::PowerBudget;
using sr::PowerShed;
using sr::Projectile;
using sr::Rig;
using sr::Target;
using sr::Vec2;
using sr::Weapon;
using sr::WeaponGroup;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace weapon_system = sr::space::weapon_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt = 1.0f / 60.0f) {
    return SystemContext{world, intents, content, dt, 0};
}

// A rig root with one weapon hardpoint aimed dead-on at a target hardpoint 100 units along +x,
// with an effectively unlimited firing arc. Returns {root, weaponHardpoint}.
std::pair<entt::entity, entt::entity> MakeArmedRig(entt::registry& registry, const Weapon& weapon) {
    const entt::entity enemyRig = registry.create();
    const entt::entity enemyHardpoint = registry.create();
    registry.emplace<WorldTransform>(enemyHardpoint, Vec2{100.0f, 0.0f}, 0.0f);

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    Target target;
    target.rig = enemyRig;
    target.hardpoint = enemyHardpoint;
    registry.emplace<Target>(root, target);

    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Weapon>(hardpoint, weapon);
    registry.emplace<FiringArc>(hardpoint, sr::kPi, 0.0f, 100.0f);
    registry.get<Rig>(root).children.push_back(hardpoint);

    return {root, hardpoint};
}

Weapon ReadyWeapon() {
    // Named fields, not positional: Weapon grew behavior-modifier fields after projectilesPerShot,
    // so a trailing positional cooldown value would land in `continuous` instead.
    Weapon weapon;
    weapon.damage = 10.0f;
    weapon.damageType = DamageType::Kinetic;
    weapon.fireIntervalSeconds = 0.5f;
    weapon.projectileSpeed = 900.0f;
    weapon.rangeUnits = 750.0f;
    weapon.spreadRadians = 0.0f;
    weapon.projectilesPerShot = 1;
    weapon.cooldown = 0.0f;
    return weapon;
}

}  // namespace

TEST_CASE("WeaponSystem fires a ready, aimed, in-range weapon with FireIntent set", "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const auto [root, hardpoint] = MakeArmedRig(registry, ReadyWeapon());
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.storage<Projectile>().size() == 1);
    const entt::entity shot = registry.view<Projectile>().front();
    const auto& projectile = registry.get<Projectile>(shot);
    CHECK(projectile.damage == Approx(10.0f));
    CHECK(projectile.damageType == DamageType::Kinetic);
    CHECK(projectile.shooter == root);
    CHECK(projectile.remainingRange == Approx(750.0f));

    CHECK(registry.get<Weapon>(hardpoint).cooldown == Approx(0.5f));
    CHECK_FALSE(registry.all_of<FireIntent>(root));
}

TEST_CASE("WeaponSystem does not fire a Docked rig's weapons, even with FireIntent set",
          "[weapon]") {
    // Regression test for architecture.md 13.3 finding H / 12.34's exclusion half: a docked rig
    // -- player or NPC -- must not fire, symmetrical with a docked rig not being a valid target.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const auto [root, hardpoint] = MakeArmedRig(registry, ReadyWeapon());
    registry.emplace<FireIntent>(root);
    registry.emplace<Docked>(root);

    weapon_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.storage<Projectile>().size() == 0);
    CHECK(registry.get<Weapon>(hardpoint).cooldown == Approx(0.0f));
}

TEST_CASE("WeaponSystem does not fire without FireIntent", "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    MakeArmedRig(registry, ReadyWeapon());

    weapon_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.storage<Projectile>().size() == 0);
}

TEST_CASE("WeaponSystem does not fire while its cooldown has not elapsed", "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    Weapon weapon = ReadyWeapon();
    weapon.cooldown = 1.0f;
    const auto [root, hardpoint] = MakeArmedRig(registry, weapon);
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f));

    CHECK(registry.storage<Projectile>().size() == 0);
    CHECK(registry.get<Weapon>(hardpoint).cooldown == Approx(1.0f - 1.0f / 60.0f));
}

TEST_CASE("WeaponSystem does not fire at a target beyond its range", "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    Weapon weapon = ReadyWeapon();
    weapon.rangeUnits = 10.0f;  // Target is 100 units out (MakeArmedRig).
    const auto [root, hardpoint] = MakeArmedRig(registry, weapon);
    (void)hardpoint;
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.storage<Projectile>().size() == 0);
}

TEST_CASE("WeaponSystem does not fire at a target outside its firing arc", "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const auto [root, hardpoint] = MakeArmedRig(registry, ReadyWeapon());
    registry.get<FiringArc>(hardpoint).halfWidthRadians = 0.1f;
    // Move the target 90 degrees off the mount's facing -- well outside a 0.1 rad arc.
    registry.get<WorldTransform>(registry.get<Target>(root).hardpoint).position =
        Vec2{0.0f, 100.0f};
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.storage<Projectile>().size() == 0);
}

TEST_CASE("WeaponSystem scales cooldown recovery by the rig's power satisfaction", "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    Weapon weapon = ReadyWeapon();
    weapon.cooldown = 1.0f;
    const auto [root, hardpoint] = MakeArmedRig(registry, weapon);
    registry.emplace<PowerBudget>(root, 50.0f, 100.0f, 0.5f);

    weapon_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK(registry.get<Weapon>(hardpoint).cooldown == Approx(0.5f));
}

TEST_CASE("WeaponSystem skips a destroyed weapon hardpoint entirely", "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    Weapon weapon = ReadyWeapon();
    weapon.cooldown = 0.3f;
    const auto [root, hardpoint] = MakeArmedRig(registry, weapon);
    registry.emplace<Destroyed>(hardpoint);
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.storage<Projectile>().size() == 0);
    CHECK(registry.get<Weapon>(hardpoint).cooldown == Approx(0.3f));
}

TEST_CASE("A shed hardpoint does not fire", "[weapon]") {
    // Regression test for architecture.md 13.3 finding F: PowerSystem's severe-overdraw branch
    // tags a browned-out hardpoint PowerShed, but nothing read it -- WeaponSystem fired it at
    // full rate regardless, so load-shedding never actually cost a hardpoint (features.md 2.9).
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    Weapon weapon = ReadyWeapon();
    weapon.cooldown = 0.3f;
    const auto [root, hardpoint] = MakeArmedRig(registry, weapon);
    registry.emplace<PowerShed>(hardpoint);
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.storage<Projectile>().size() == 0);
    // Cooldown does not tick down either -- a shed hardpoint is offline, not merely slow.
    CHECK(registry.get<Weapon>(hardpoint).cooldown == Approx(0.3f));
}

TEST_CASE("WeaponSystem fires at AimPoint rather than at Target when the rig has one", "[weapon]") {
    // Regression for features.md 3.2 / architecture.md 12.24 step 2: the player aims manually --
    // a shot must go where the cursor is, not where TargetingSystem's acquisition would have
    // aimed a seeker.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    // Target still names a hardpoint far off-axis (90 degrees, well outside the arc below); the
    // shot should ignore it entirely and hit the AimPoint straight ahead instead.
    const auto [root, hardpoint] = MakeArmedRig(registry, ReadyWeapon());
    registry.get<FiringArc>(hardpoint).halfWidthRadians = 0.1f;
    registry.emplace<AimPoint>(root, Vec2{100.0f, 0.0f});
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.storage<Projectile>().size() == 1);
}

TEST_CASE("WeaponSystem fires at AimPoint even with no Target acquired at all", "[weapon]") {
    // Regression for the second half of the same bug: "a player with no hostile in sensor range
    // could not fire at all, because WeaponSystem skips any rig whose Target is null."
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<Target>(root);  // Default-constructed: rig == entt::null.
    registry.emplace<AimPoint>(root, Vec2{100.0f, 0.0f});

    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Weapon>(hardpoint, ReadyWeapon());
    registry.emplace<FiringArc>(hardpoint, sr::kPi, 0.0f, 100.0f);
    registry.get<Rig>(root).children.push_back(hardpoint);

    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.storage<Projectile>().size() == 1);
}

TEST_CASE("WeaponSystem only fires hardpoints whose weapon group is enabled", "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const auto [root, groupZeroHardpoint] = MakeArmedRig(registry, ReadyWeapon());
    registry.emplace<WeaponGroup>(groupZeroHardpoint, std::uint8_t{0});

    const entt::entity groupOneHardpoint = registry.create();
    registry.emplace<WorldTransform>(groupOneHardpoint, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Weapon>(groupOneHardpoint, ReadyWeapon());
    registry.emplace<FiringArc>(groupOneHardpoint, sr::kPi, 0.0f, 100.0f);
    registry.emplace<WeaponGroup>(groupOneHardpoint, std::uint8_t{1});
    registry.get<Rig>(root).children.push_back(groupOneHardpoint);

    // Only group 1 enabled -- group 0's mount must hold fire even though it is otherwise ready.
    registry.emplace<EnabledWeaponGroups>(root, std::uint16_t{0b10});
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.storage<Projectile>().size() == 1);
    const entt::entity shot = registry.view<Projectile>().front();
    CHECK(registry.get<Weapon>(groupZeroHardpoint).cooldown == Approx(0.0f));
    CHECK(registry.get<Weapon>(groupOneHardpoint).cooldown == Approx(0.5f));
    (void)shot;
}

TEST_CASE("A freshly spawned aegis_vanguard fires its wing cannons dead ahead", "[weapon]") {
    // Full-pipeline regression: blueprint JSON -> RigFactory -> WeaponSystem, the exact path a
    // real player takes, rather than WeaponSystemTests' hand-built MakeArmedRig fixture. Isolates
    // whether "no projectiles fire in game" is a WeaponSystem bug or an input/front-end one.
    sr::core::ContentLibrary content;
    const auto report = content.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());

    SystemWorld world("sol");
    rig_factory::SpawnParams params;
    params.blueprint = sr::BlueprintId("aegis_vanguard");
    params.position = {0.0f, 0.0f};
    params.rotation = 0.0f;
    const auto spawned = rig_factory::Spawn(world, content, params);
    REQUIRE(spawned.ok());

    entt::registry& registry = world.Registry();
    registry.emplace<AimPoint>(spawned.root, Vec2{500.0f, 0.0f});  // Dead ahead of rotation 0.
    registry.emplace<FireIntent>(spawned.root);

    sr::core::IntentQueue intents;
    // Run several ticks: FiringArc must slew from its spawned currentOffset (0.0) to the aim
    // point before AimAt reports on-target, the same as a real frame sequence would.
    for (int i = 0; i < 10; ++i) {
        weapon_system::Tick(MakeContext(world, intents, content));
        registry.emplace_or_replace<FireIntent>(spawned.root);
    }

    CHECK(registry.storage<Projectile>().size() > 0);
}

TEST_CASE("A chargeToFire+homing weapon fires once charge completes and the lock holds",
          "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity enemyRig = registry.create();
    const entt::entity enemyHardpoint = registry.create();
    registry.emplace<WorldTransform>(enemyHardpoint, Vec2{100.0f, 0.0f}, 0.0f);
    registry.emplace<sr::HitRadius>(enemyHardpoint, 5.0f);
    registry.emplace<sr::ParentRig>(enemyHardpoint, enemyRig);

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<Target>(root);
    registry.emplace<AimPoint>(root, Vec2{100.0f, 0.0f});

    // lockon_missile_i's exact authored stats (data/base_game/modules.json), plus explicit
    // consistency=1.0/accuracyRadians=0.0 (their own defaults) so this fixture does not silently
    // drift if Weapon ever grows another field between accuracyRadians and cooldown.
    Weapon lockOnMissile{40.0f,   DamageType::Kinetic,
                         0.5f,    650.0f,
                         1000.0f, 0.0f,
                         1,       false,
                         4.0f,    true,
                         1.2f,    1,
                         0.0f,    6.0f,
                         1.0f,    0.0f,
                         0.0f,    6.0f};
    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Weapon>(hardpoint, lockOnMissile);
    registry.emplace<FiringArc>(hardpoint, sr::kPi, 0.0f, 100.0f);
    registry.get<Rig>(root).children.push_back(hardpoint);

    for (int i = 0; i < 120; ++i) {
        registry.emplace_or_replace<FireIntent>(root);
        weapon_system::Tick(MakeContext(world, intents, content));
    }

    CHECK(registry.get<Weapon>(hardpoint).lockedTarget == enemyHardpoint);
    CHECK(registry.get<Weapon>(hardpoint).chargeSeconds == Approx(1.2f));
    REQUIRE(registry.storage<Projectile>().size() >= 1);
}

TEST_CASE("A chargeToFire+homing weapon keeps its lock but holds fire once ammo depletes",
          "[weapon]") {
    // Regression for the exact symptom a chargeToFire+homing weapon presents once its rack runs
    // dry: charge/lock still succeed (they do not consume ammo), but ReadyToFire's outOfAmmo gate
    // blocks the final shot -- "locks onto a target but does not fire anything" is the correct,
    // by-design behavior for maxAmmo reaching 0, not a bug, since there is no reload mechanic
    // (WeaponStats::maxAmmo's own comment). This proves the lock/charge readout alone cannot tell
    // the two apart -- only the ammo readout can.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity enemyRig = registry.create();
    const entt::entity enemyHardpoint = registry.create();
    registry.emplace<WorldTransform>(enemyHardpoint, Vec2{100.0f, 0.0f}, 0.0f);
    registry.emplace<sr::HitRadius>(enemyHardpoint, 5.0f);
    registry.emplace<sr::ParentRig>(enemyHardpoint, enemyRig);

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<Target>(root);
    registry.emplace<AimPoint>(root, Vec2{100.0f, 0.0f});

    Weapon lockOnMissile{40.0f,   DamageType::Kinetic,
                         0.5f,    650.0f,
                         1000.0f, 0.0f,
                         1,       false,
                         4.0f,    true,
                         1.2f,    1,
                         0.0f,    6.0f,
                         1.0f,    0.0f,
                         0.0f,    0.0f};
    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Weapon>(hardpoint, lockOnMissile);  // ammoRemaining pre-set to 0 above.
    registry.emplace<FiringArc>(hardpoint, sr::kPi, 0.0f, 100.0f);
    registry.get<Rig>(root).children.push_back(hardpoint);

    for (int i = 0; i < 120; ++i) {
        registry.emplace_or_replace<FireIntent>(root);
        weapon_system::Tick(MakeContext(world, intents, content));
    }

    CHECK(registry.get<Weapon>(hardpoint).lockedTarget == enemyHardpoint);
    CHECK(registry.get<Weapon>(hardpoint).chargeSeconds == Approx(1.2f));
    CHECK(registry.storage<Projectile>().size() == 0);
}

TEST_CASE("A weapon with consistency 0 deviates a shot away from the aim direction", "[weapon]") {
    // Regression for WeaponStats::consistency/accuracyRadians: consistency=0 forces every shot
    // onto the "miss" branch, so its fired direction must differ from the dead-on bearing every
    // other test's default consistency=1 fixtures always produce.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    Weapon weapon = ReadyWeapon();
    weapon.consistency = 0.0f;
    weapon.accuracyRadians = 1.0f;
    const auto [root, hardpoint] = MakeArmedRig(registry, weapon);
    (void)hardpoint;
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.storage<Projectile>().size() == 1);
    const entt::entity shot = registry.view<Projectile>().front();
    // 0.0 is the dead-on bearing toward the target at (100, 0) from the mount at (0, 0).
    CHECK(registry.get<WorldTransform>(shot).rotation != Approx(0.0f));
}

TEST_CASE("A continuous weapon queues damage scaled by dt and satisfaction, and leaves a BeamState",
          "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity enemyRig = registry.create();
    const entt::entity enemyHardpoint = registry.create();
    registry.emplace<WorldTransform>(enemyHardpoint, Vec2{100.0f, 0.0f}, 0.0f);
    registry.emplace<sr::HitRadius>(enemyHardpoint, 5.0f);
    registry.emplace<sr::ParentRig>(enemyHardpoint, enemyRig);

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<Target>(root);
    registry.emplace<AimPoint>(root, Vec2{100.0f, 0.0f});
    registry.emplace<PowerBudget>(root, 50.0f, 100.0f, 0.5f);

    Weapon weapon = ReadyWeapon();
    weapon.continuous = true;
    weapon.damage = 100.0f;  // Per second (WeaponStats::continuous's own comment), not per hit.

    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Weapon>(hardpoint, weapon);
    registry.emplace<FiringArc>(hardpoint, sr::kPi, 0.0f, 100.0f);
    registry.get<Rig>(root).children.push_back(hardpoint);
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content, 1.0f));

    // No Projectile entity: a beam has no travel time (WeaponStats::continuous's own comment).
    CHECK(registry.storage<Projectile>().size() == 0);
    REQUIRE(registry.all_of<sr::PendingDamage>(enemyHardpoint));
    const auto& pending = registry.get<sr::PendingDamage>(enemyHardpoint);
    CHECK(pending.amount == Approx(50.0f));  // 100/s * 1.0s * 0.5 satisfaction.
    CHECK(pending.type == DamageType::Kinetic);
    CHECK(pending.source == root);
    REQUIRE(registry.all_of<sr::BeamState>(hardpoint));
    CHECK(registry.get<sr::BeamState>(hardpoint).endPoint.x == Approx(100.0f));
}

TEST_CASE("A continuous weapon's BeamState is cleared the tick it stops firing", "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<Target>(root);
    registry.emplace<AimPoint>(root, Vec2{100.0f, 0.0f});

    Weapon weapon = ReadyWeapon();
    weapon.continuous = true;

    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Weapon>(hardpoint, weapon);
    registry.emplace<FiringArc>(hardpoint, sr::kPi, 0.0f, 100.0f);
    registry.get<Rig>(root).children.push_back(hardpoint);
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f));
    REQUIRE(registry.all_of<sr::BeamState>(hardpoint));

    // FireIntent already cleared itself at the end of the previous Tick (Tick's own trailing
    // registry.clear<FireIntent>()) -- this tick genuinely has none, the same as a released
    // trigger.
    weapon_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f));

    CHECK_FALSE(registry.all_of<sr::BeamState>(hardpoint));
}

TEST_CASE("A continuous weapon with finite ammo drains it by dt per second of fire, not per shot",
          "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<Target>(root);
    registry.emplace<AimPoint>(root, Vec2{100.0f, 0.0f});

    Weapon weapon = ReadyWeapon();
    weapon.continuous = true;
    weapon.maxAmmo = 10.0f;
    weapon.ammoRemaining = 10.0f;

    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Weapon>(hardpoint, weapon);
    registry.emplace<FiringArc>(hardpoint, sr::kPi, 0.0f, 100.0f);
    registry.get<Rig>(root).children.push_back(hardpoint);
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content, 2.0f));

    CHECK(registry.get<Weapon>(hardpoint).ammoRemaining == Approx(8.0f));
}

TEST_CASE(
    "A burstCount weapon fires its shots burstIntervalSeconds apart and withholds the normal "
    "cooldown until the burst finishes",
    "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    Weapon weapon = ReadyWeapon();
    weapon.burstCount = 3;
    weapon.burstIntervalSeconds = 0.1f;
    weapon.fireIntervalSeconds = 1.0f;
    const auto [root, hardpoint] = MakeArmedRig(registry, weapon);
    registry.emplace<FireIntent>(root);

    // First tick fires shot 1 of 3 immediately (FireDiscrete) and starts the burst's own timer.
    weapon_system::Tick(MakeContext(world, intents, content, 0.0f));
    CHECK(registry.storage<Projectile>().size() == 1);
    CHECK(registry.get<Weapon>(hardpoint).burstShotsRemaining == 2);
    CHECK(registry.get<Weapon>(hardpoint).cooldown == Approx(0.0f));  // Withheld mid-burst.

    // FireIntent is not re-applied from here -- a burst plays out on its own timer "regardless of
    // continued FireIntent/charge state" (WeaponStats::burstCount's own comment).
    weapon_system::Tick(
        MakeContext(world, intents, content, 0.1f));  // Elapses burstIntervalSeconds.
    CHECK(registry.storage<Projectile>().size() == 2);
    CHECK(registry.get<Weapon>(hardpoint).burstShotsRemaining == 1);
    CHECK(registry.get<Weapon>(hardpoint).cooldown == Approx(0.0f));

    weapon_system::Tick(MakeContext(world, intents, content, 0.1f));
    CHECK(registry.storage<Projectile>().size() == 3);
    CHECK(registry.get<Weapon>(hardpoint).burstShotsRemaining == 0);
    CHECK(registry.get<Weapon>(hardpoint).cooldown == Approx(1.0f));  // Set on the final shot.
}

TEST_CASE("A burst stops early if ammo runs out mid-burst", "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    Weapon weapon = ReadyWeapon();
    weapon.burstCount = 3;
    weapon.burstIntervalSeconds = 0.1f;
    weapon.maxAmmo = 2.0f;
    weapon.ammoRemaining = 2.0f;
    const auto [root, hardpoint] = MakeArmedRig(registry, weapon);
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content, 0.0f));  // Shot 1/3, ammo 2 -> 1.
    CHECK(registry.storage<Projectile>().size() == 1);
    CHECK(registry.get<Weapon>(hardpoint).burstShotsRemaining == 2);

    weapon_system::Tick(MakeContext(world, intents, content, 0.1f));  // Shot 2/3, ammo 1 -> 0.
    CHECK(registry.storage<Projectile>().size() == 2);
    CHECK(registry.get<Weapon>(hardpoint).burstShotsRemaining == 1);

    // Shot 3 would be due, but ammoRemaining is now 0 -- TickBurst's own outOfAmmo branch ends
    // the burst without firing it and applies the normal cooldown anyway.
    weapon_system::Tick(MakeContext(world, intents, content, 0.1f));
    CHECK(registry.storage<Projectile>().size() == 2);
    CHECK(registry.get<Weapon>(hardpoint).burstShotsRemaining == 0);
    CHECK(registry.get<Weapon>(hardpoint).cooldown == Approx(weapon.fireIntervalSeconds));
}

TEST_CASE(
    "A plain finite-ammo weapon stops firing once ammo depletes, with no charge or lock involved",
    "[weapon]") {
    // The chargeToFire+homing lock-on tests already cover ammo depletion for THAT combination;
    // this covers the far more common case of an ordinary discrete weapon with a limited rack.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    Weapon weapon = ReadyWeapon();
    weapon.fireIntervalSeconds = 0.0f;  // Ready again immediately, so only ammo gates the 3rd shot.
    weapon.maxAmmo = 2.0f;
    weapon.ammoRemaining = 2.0f;
    const auto [root, hardpoint] = MakeArmedRig(registry, weapon);

    registry.emplace<FireIntent>(root);
    weapon_system::Tick(MakeContext(world, intents, content, 0.0f));
    CHECK(registry.storage<Projectile>().size() == 1);
    CHECK(registry.get<Weapon>(hardpoint).ammoRemaining == Approx(1.0f));

    registry.emplace<FireIntent>(root);
    weapon_system::Tick(MakeContext(world, intents, content, 0.0f));
    CHECK(registry.storage<Projectile>().size() == 2);
    CHECK(registry.get<Weapon>(hardpoint).ammoRemaining == Approx(0.0f));

    registry.emplace<FireIntent>(root);
    weapon_system::Tick(MakeContext(world, intents, content, 0.0f));
    CHECK(registry.storage<Projectile>().size() == 2);  // Out of ammo -- no third shot.
}

TEST_CASE(
    "A continuous+homing weapon with no charge aims at the nearest eligible hardpoint on its own, "
    "ignoring the cursor",
    "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity enemyRig = registry.create();
    const entt::entity enemyHardpoint = registry.create();
    // 90 degrees off the mount's forward axis, and far from where AimPoint below points -- only
    // an actual auto-seek (ResolveAimPoint's continuous+homing branch), not the cursor, can steer
    // a shot here.
    registry.emplace<WorldTransform>(enemyHardpoint, Vec2{0.0f, 100.0f}, 0.0f);
    registry.emplace<sr::HitRadius>(enemyHardpoint, 5.0f);
    registry.emplace<sr::ParentRig>(enemyHardpoint, enemyRig);

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<Target>(root);
    registry.emplace<AimPoint>(root, Vec2{500.0f, 0.0f});  // Cursor points dead ahead instead.

    Weapon weapon = ReadyWeapon();
    weapon.continuous = true;
    weapon.homingTurnRatePerSecond = sr::kPi;
    weapon.rangeUnits = 200.0f;

    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Weapon>(hardpoint, weapon);
    registry.emplace<FiringArc>(hardpoint, sr::kPi, 0.0f, 100.0f);
    registry.get<Rig>(root).children.push_back(hardpoint);

    // Several ticks: the arc must slew from its spawned currentOffset (0.0) to the auto-seek
    // target's bearing (+90 degrees) before it comes on-target.
    for (int i = 0; i < 10; ++i) {
        registry.emplace_or_replace<FireIntent>(root);
        weapon_system::Tick(MakeContext(world, intents, content));
    }

    REQUIRE(registry.all_of<sr::PendingDamage>(enemyHardpoint));
}

TEST_CASE("Releasing a chargeToFire+homing weapon's trigger resets its charge and lock",
          "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity enemyRig = registry.create();
    const entt::entity enemyHardpoint = registry.create();
    registry.emplace<WorldTransform>(enemyHardpoint, Vec2{100.0f, 0.0f}, 0.0f);
    registry.emplace<sr::HitRadius>(enemyHardpoint, 5.0f);
    registry.emplace<sr::ParentRig>(enemyHardpoint, enemyRig);

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<Target>(root);
    registry.emplace<AimPoint>(root, Vec2{100.0f, 0.0f});

    Weapon weapon = ReadyWeapon();
    weapon.chargeToFire = true;
    weapon.chargeSecondsToFire = 2.0f;
    weapon.homingTurnRatePerSecond = sr::kPi;

    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Weapon>(hardpoint, weapon);
    registry.emplace<FiringArc>(hardpoint, sr::kPi, 0.0f, 100.0f);
    registry.get<Rig>(root).children.push_back(hardpoint);

    registry.emplace<FireIntent>(root);
    weapon_system::Tick(MakeContext(world, intents, content, 1.0f));  // Charges 1s of 2s, locks on.

    REQUIRE(registry.get<Weapon>(hardpoint).lockedTarget == enemyHardpoint);
    REQUIRE(registry.get<Weapon>(hardpoint).chargeSeconds == Approx(1.0f));

    // FireIntent already cleared itself at the end of the previous Tick -- this tick genuinely has
    // none, the same as a released trigger.
    weapon_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK(registry.get<Weapon>(hardpoint).chargeSeconds == Approx(0.0f));
    CHECK((registry.get<Weapon>(hardpoint).lockedTarget == entt::null));
    CHECK(registry.storage<Projectile>().size() == 0);
}

TEST_CASE(
    "A chargeToFire+homing weapon's lock breaks and resets if the locked target dies mid-charge",
    "[weapon]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity enemyRig = registry.create();
    const entt::entity enemyHardpoint = registry.create();
    registry.emplace<WorldTransform>(enemyHardpoint, Vec2{100.0f, 0.0f}, 0.0f);
    registry.emplace<sr::HitRadius>(enemyHardpoint, 5.0f);
    registry.emplace<sr::ParentRig>(enemyHardpoint, enemyRig);

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<Target>(root);
    registry.emplace<AimPoint>(root, Vec2{100.0f, 0.0f});

    Weapon weapon = ReadyWeapon();
    weapon.chargeToFire = true;
    weapon.chargeSecondsToFire = 2.0f;
    weapon.homingTurnRatePerSecond = sr::kPi;

    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Weapon>(hardpoint, weapon);
    registry.emplace<FiringArc>(hardpoint, sr::kPi, 0.0f, 100.0f);
    registry.get<Rig>(root).children.push_back(hardpoint);

    registry.emplace<FireIntent>(root);
    weapon_system::Tick(MakeContext(world, intents, content, 1.0f));
    REQUIRE(registry.get<Weapon>(hardpoint).lockedTarget == enemyHardpoint);

    registry.emplace<Destroyed>(enemyHardpoint);
    registry.emplace<FireIntent>(root);  // Still held.

    weapon_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK(registry.get<Weapon>(hardpoint).chargeSeconds == Approx(0.0f));
    CHECK((registry.get<Weapon>(hardpoint).lockedTarget == entt::null));
    CHECK(registry.storage<Projectile>().size() == 0);
}

TEST_CASE("A hardpoint with no WeaponGroup fires regardless of the rig's enabled mask",
          "[weapon]") {
    // Fail-open: a runtime-mounted weapon that has not been assigned a group yet (out of this
    // issue's scope -- ModuleEquipSystem does not assign one) must not go permanently silent.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const auto [root, hardpoint] = MakeArmedRig(registry, ReadyWeapon());
    registry.emplace<EnabledWeaponGroups>(root, std::uint16_t{0});  // Every defined group off.
    registry.emplace<FireIntent>(root);

    weapon_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.storage<Projectile>().size() == 1);
}
