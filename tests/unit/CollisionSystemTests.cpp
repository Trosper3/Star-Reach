#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/CollisionSystem.h"
#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"

using Catch::Approx;
using sr::BodyMass;
using sr::CollisionRadius;
using sr::Destroyed;
using sr::Health;
using sr::HitRadius;
using sr::ParentRig;
using sr::PendingDamage;
using sr::RamCooldown;
using sr::Rig;
using sr::Vec2;
using sr::Velocity;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace collision_system = sr::space::collision_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt = 1.0f / 60.0f) {
    return SystemContext{world, intents, content, dt, 0};
}

// A rig root with one hardpoint sitting at the root's own position -- close enough to a circle,
// via BuildWorldHull's octagon sampling, to exercise SAT overlap without needing real geometry.
entt::entity MakeRig(entt::registry& registry, Vec2 position, Vec2 velocity, float mass,
                     float radius) {
    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, position, 0.0f);
    registry.emplace<Velocity>(root, velocity, 0.0f);
    registry.emplace<BodyMass>(root, mass);
    registry.emplace<CollisionRadius>(root, radius);
    registry.emplace<RamCooldown>(root);

    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, position, 0.0f);
    registry.emplace<ParentRig>(hardpoint, root);
    registry.emplace<HitRadius>(hardpoint, radius);
    registry.emplace<Health>(hardpoint, 100.0f, 100.0f);

    Rig rig;
    rig.children.push_back(hardpoint);
    registry.emplace<Rig>(root, std::move(rig));

    return root;
}

}  // namespace

TEST_CASE("CollisionSystem leaves rigs whose broad-phase circles do not overlap untouched",
          "[collision]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity a = MakeRig(registry, Vec2{0.0f, 0.0f}, Vec2{5.0f, 0.0f}, 1000.0f, 50.0f);
    const entt::entity b = MakeRig(registry, Vec2{500.0f, 0.0f}, Vec2{-5.0f, 0.0f}, 1000.0f, 50.0f);

    collision_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<WorldTransform>(a).position.x == Approx(0.0f));
    CHECK(registry.get<WorldTransform>(b).position.x == Approx(500.0f));
    CHECK(registry.get<Velocity>(a).linear.x == Approx(5.0f));
    CHECK(registry.storage<PendingDamage>().size() == 0);
}

TEST_CASE(
    "CollisionSystem keeps the heavier rig fixed, pushes the lighter one clear, and queues "
    "asymmetric ramming damage",
    "[collision]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity heavy =
        MakeRig(registry, Vec2{-30.0f, 0.0f}, Vec2{5.0f, 0.0f}, 2000.0f, 50.0f);
    const entt::entity light =
        MakeRig(registry, Vec2{30.0f, 0.0f}, Vec2{-5.0f, 0.0f}, 500.0f, 50.0f);

    collision_system::Tick(MakeContext(world, intents, content));

    // The heavier side never moves or changes velocity in an asymmetric mass/momentum bounce.
    CHECK(registry.get<WorldTransform>(heavy).position.x == Approx(-30.0f));
    CHECK(registry.get<Velocity>(heavy).linear.x == Approx(5.0f));

    // The lighter side is pushed clear of the heavier one and bounces off it.
    CHECK(registry.get<WorldTransform>(light).position.x > 30.0f);
    CHECK(registry.get<Velocity>(light).linear.x > -5.0f);

    // Ramming damage lands on each rig's hardpoint, split so the heavier side takes less.
    const entt::entity heavyHardpoint = registry.get<Rig>(heavy).children.front();
    const entt::entity lightHardpoint = registry.get<Rig>(light).children.front();
    REQUIRE(registry.all_of<PendingDamage>(heavyHardpoint));
    REQUIRE(registry.all_of<PendingDamage>(lightHardpoint));
    const float heavyDamage = registry.get<PendingDamage>(heavyHardpoint).amount;
    const float lightDamage = registry.get<PendingDamage>(lightHardpoint).amount;
    CHECK(heavyDamage > 0.0f);
    CHECK(lightDamage > heavyDamage);

    CHECK(registry.get<RamCooldown>(heavy).secondsRemaining == Approx(1.2f));
    CHECK(registry.get<RamCooldown>(light).secondsRemaining == Approx(1.2f));
}

TEST_CASE("CollisionSystem does not queue ramming damage while RamCooldown is still active",
          "[collision]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity a = MakeRig(registry, Vec2{-30.0f, 0.0f}, Vec2{5.0f, 0.0f}, 1000.0f, 50.0f);
    const entt::entity b = MakeRig(registry, Vec2{30.0f, 0.0f}, Vec2{-5.0f, 0.0f}, 1000.0f, 50.0f);
    registry.get<RamCooldown>(a).secondsRemaining = 1.0f;

    collision_system::Tick(MakeContext(world, intents, content));

    const entt::entity hardpointA = registry.get<Rig>(a).children.front();
    const entt::entity hardpointB = registry.get<Rig>(b).children.front();
    CHECK_FALSE(registry.all_of<PendingDamage>(hardpointA));
    CHECK_FALSE(registry.all_of<PendingDamage>(hardpointB));
}

TEST_CASE("CollisionSystem does not collide a rig whose only hardpoint is destroyed",
          "[collision]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity wreck =
        MakeRig(registry, Vec2{-30.0f, 0.0f}, Vec2{5.0f, 0.0f}, 1000.0f, 50.0f);
    registry.emplace<Destroyed>(registry.get<Rig>(wreck).children.front());
    const entt::entity live =
        MakeRig(registry, Vec2{30.0f, 0.0f}, Vec2{-5.0f, 0.0f}, 1000.0f, 50.0f);

    collision_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<WorldTransform>(wreck).position.x == Approx(-30.0f));
    CHECK(registry.get<WorldTransform>(live).position.x == Approx(30.0f));
    CHECK(registry.get<Velocity>(live).linear.x == Approx(-5.0f));
}
