#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/ProjectileSystem.h"
#include "shared/components/Combat.h"
#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"

using Catch::Approx;
using sr::DamageType;
using sr::HitRadius;
using sr::ParentRig;
using sr::PendingDamage;
using sr::PreviousTransform;
using sr::Projectile;
using sr::Vec2;
using sr::Velocity;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace projectile_system = sr::space::projectile_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt = 1.0f / 60.0f) {
    return SystemContext{world, intents, content, dt, 0};
}

entt::entity MakeProjectile(entt::registry& registry, const Vec2& position, const Vec2& velocity,
                            float damage, DamageType type, entt::entity shooter,
                            float remainingRange) {
    const entt::entity entity = registry.create();
    registry.emplace<WorldTransform>(entity, position, 0.0f);
    registry.emplace<PreviousTransform>(entity, position, 0.0f);
    registry.emplace<Velocity>(entity, velocity, 0.0f);
    registry.emplace<Projectile>(entity, damage, type, shooter, remainingRange);
    return entity;
}

entt::entity MakeHardpoint(entt::registry& registry, entt::entity root, const Vec2& position,
                           float radius) {
    const entt::entity entity = registry.create();
    registry.emplace<WorldTransform>(entity, position, 0.0f);
    registry.emplace<HitRadius>(entity, radius);
    registry.emplace<ParentRig>(entity, root);
    return entity;
}

}  // namespace

TEST_CASE("ProjectileSystem advances a projectile and shrinks its remaining range",
          "[projectile]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity shot = MakeProjectile(registry, Vec2{0.0f, 0.0f}, Vec2{600.0f, 0.0f}, 10.0f,
                                             DamageType::Kinetic, entt::null, 1000.0f);

    projectile_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f));

    REQUIRE(registry.valid(shot));
    CHECK(registry.get<WorldTransform>(shot).position.x == Approx(10.0f));
    CHECK(registry.get<Projectile>(shot).remainingRange == Approx(990.0f));
}

TEST_CASE("ProjectileSystem culls a projectile once its remaining range is exhausted",
          "[projectile]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity shot = MakeProjectile(registry, Vec2{0.0f, 0.0f}, Vec2{600.0f, 0.0f}, 10.0f,
                                             DamageType::Kinetic, entt::null, 5.0f);

    projectile_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f));

    CHECK_FALSE(registry.valid(shot));
}

TEST_CASE("ProjectileSystem hits a hostile hardpoint in its path, queues damage, and culls itself",
          "[projectile]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity shooterRig = registry.create();
    const entt::entity targetRig = registry.create();
    const entt::entity hardpoint = MakeHardpoint(registry, targetRig, Vec2{50.0f, 0.0f}, 10.0f);
    // 3000 units/s at 1/60 s covers exactly 50 units -- lands on the hardpoint's center.
    const entt::entity shot = MakeProjectile(registry, Vec2{0.0f, 0.0f}, Vec2{3000.0f, 0.0f}, 25.0f,
                                             DamageType::Energy, shooterRig, 1000.0f);

    projectile_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f));

    CHECK_FALSE(registry.valid(shot));
    REQUIRE(registry.all_of<PendingDamage>(hardpoint));
    const auto& pending = registry.get<PendingDamage>(hardpoint);
    CHECK(pending.amount == Approx(25.0f));
    CHECK(pending.type == DamageType::Energy);
    CHECK(pending.source == shooterRig);
}

TEST_CASE("ProjectileSystem does not hit a hardpoint on the shooter's own rig", "[projectile]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity shooterRig = registry.create();
    const entt::entity hardpoint = MakeHardpoint(registry, shooterRig, Vec2{50.0f, 0.0f}, 10.0f);
    const entt::entity shot = MakeProjectile(registry, Vec2{0.0f, 0.0f}, Vec2{3000.0f, 0.0f}, 25.0f,
                                             DamageType::Energy, shooterRig, 100000.0f);

    projectile_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f));

    CHECK(registry.valid(shot));
    CHECK_FALSE(registry.all_of<PendingDamage>(hardpoint));
}

TEST_CASE("ProjectileSystem detects a hit along its swept path, not just at its endpoint",
          "[projectile]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity shooterRig = registry.create();
    const entt::entity targetRig = registry.create();
    // The path from (0,0) to (50,0) passes directly through (30,0); the endpoint alone
    // (distance 20 from a radius-5 target) would miss a point-only check.
    const entt::entity hardpoint = MakeHardpoint(registry, targetRig, Vec2{30.0f, 0.0f}, 5.0f);
    const entt::entity shot = MakeProjectile(registry, Vec2{0.0f, 0.0f}, Vec2{3000.0f, 0.0f}, 25.0f,
                                             DamageType::Kinetic, shooterRig, 1000.0f);

    projectile_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f));

    CHECK_FALSE(registry.valid(shot));
    CHECK(registry.all_of<PendingDamage>(hardpoint));
}

TEST_CASE("ProjectileSystem accumulates damage from two hits on the same hardpoint in one tick",
          "[projectile]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity shooterA = registry.create();
    const entt::entity shooterB = registry.create();
    const entt::entity targetRig = registry.create();
    const entt::entity hardpoint = MakeHardpoint(registry, targetRig, Vec2{50.0f, 0.0f}, 10.0f);

    MakeProjectile(registry, Vec2{0.0f, 0.0f}, Vec2{3000.0f, 0.0f}, 10.0f, DamageType::Kinetic,
                   shooterA, 1000.0f);
    MakeProjectile(registry, Vec2{0.0f, 5.0f}, Vec2{3000.0f, 0.0f}, 15.0f, DamageType::Kinetic,
                   shooterB, 1000.0f);

    projectile_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f));

    REQUIRE(registry.all_of<PendingDamage>(hardpoint));
    CHECK(registry.get<PendingDamage>(hardpoint).amount == Approx(25.0f));
    CHECK(registry.storage<Projectile>().size() == 0);
}
