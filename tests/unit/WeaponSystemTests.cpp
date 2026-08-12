#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/WeaponSystem.h"
#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"

using Catch::Approx;
using sr::DamageType;
using sr::Destroyed;
using sr::Docked;
using sr::FireIntent;
using sr::FiringArc;
using sr::PowerBudget;
using sr::PowerShed;
using sr::Projectile;
using sr::Rig;
using sr::Target;
using sr::Vec2;
using sr::Weapon;
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
    return Weapon{10.0f, DamageType::Kinetic, 0.5f, 900.0f, 750.0f, 0.0f, 1, 0.0f};
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
