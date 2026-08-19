#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/PhysicsSystem.h"
#include "shared/components/Physics.h"
#include "shared/components/Power.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"

using Catch::Approx;
using sr::BodyMass;
using sr::LinearDamping;
using sr::PowerBudget;
using sr::PreviousTransform;
using sr::Propulsion;
using sr::ThrustInput;
using sr::Vec2;
using sr::Velocity;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace physics_system = sr::space::physics_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt = 1.0f / 60.0f) {
    return SystemContext{world, intents, content, dt, 0};
}

}  // namespace

TEST_CASE("PhysicsSystem accelerates a rig root along its heading under full forward thrust",
          "[physics]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Velocity>(root);
    registry.emplace<BodyMass>(root, 10.0f);
    registry.emplace<Propulsion>(root, 100.0f, 0.0f, 1000.0f);
    registry.emplace<ThrustInput>(root, 1.0f, 0.0f, 0.0f);

    physics_system::Tick(MakeContext(world, intents, content, 1.0f));

    // a = F / m = 100 / 10 = 10 m/s^2; over dt = 1s, v = 10 and heading is +x at rotation 0.
    const auto& velocity = registry.get<Velocity>(root);
    CHECK(velocity.linear.x == Approx(10.0f));
    CHECK(velocity.linear.y == Approx(0.0f).margin(0.0001f));

    const auto& transform = registry.get<WorldTransform>(root);
    CHECK(transform.position.x == Approx(10.0f));
}

TEST_CASE("PhysicsSystem scales thrust by the rig's PowerBudget::engines", "[physics]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Velocity>(root);
    registry.emplace<BodyMass>(root, 10.0f);
    registry.emplace<Propulsion>(root, 100.0f, 0.0f, 1000.0f);
    registry.emplace<ThrustInput>(root, 1.0f, 0.0f, 0.0f);
    // generation, draw, weapons, shields, engines -- only `engines` matters to PhysicsSystem.
    registry.emplace<PowerBudget>(root, 50.0f, 100.0f, 1.0f, 1.0f, 0.5f);

    physics_system::Tick(MakeContext(world, intents, content, 1.0f));

    const auto& velocity = registry.get<Velocity>(root);
    CHECK(velocity.linear.x == Approx(5.0f));
}

TEST_CASE("PhysicsSystem clamps speed to the rig's maxSpeed", "[physics]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Velocity>(root, Vec2{40.0f, 0.0f}, 0.0f);
    registry.emplace<BodyMass>(root, 10.0f);
    registry.emplace<Propulsion>(root, 100.0f, 0.0f, 30.0f);
    registry.emplace<ThrustInput>(root, 1.0f, 0.0f, 0.0f);

    physics_system::Tick(MakeContext(world, intents, content, 1.0f));

    const auto& velocity = registry.get<Velocity>(root);
    CHECK(velocity.linear.x == Approx(30.0f));
}

TEST_CASE("PhysicsSystem bleeds velocity off with LinearDamping and no thrust", "[physics]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Velocity>(root, Vec2{10.0f, 0.0f}, 4.0f);
    registry.emplace<BodyMass>(root, 10.0f);
    registry.emplace<LinearDamping>(root, 0.5f, 0.25f);

    physics_system::Tick(MakeContext(world, intents, content, 1.0f));

    const auto& velocity = registry.get<Velocity>(root);
    CHECK(velocity.linear.x == Approx(5.0f));
    CHECK(velocity.angular == Approx(3.0f));
}

TEST_CASE("PhysicsSystem turns a rig root under turn input", "[physics]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Velocity>(root);
    registry.emplace<BodyMass>(root, 10.0f);
    registry.emplace<Propulsion>(root, 0.0f, 5.0f, 1000.0f);
    registry.emplace<ThrustInput>(root, 0.0f, 0.0f, 1.0f);

    physics_system::Tick(MakeContext(world, intents, content, 1.0f));

    const auto& velocity = registry.get<Velocity>(root);
    CHECK(velocity.angular == Approx(0.5f));

    const auto& transform = registry.get<WorldTransform>(root);
    CHECK(transform.rotation == Approx(0.5f));
}

TEST_CASE("PhysicsSystem records the root's prior WorldTransform into PreviousTransform",
          "[physics]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{1.0f, 2.0f}, 0.3f);
    registry.emplace<PreviousTransform>(root, Vec2{1.0f, 2.0f}, 0.3f);
    registry.emplace<Velocity>(root, Vec2{5.0f, 0.0f}, 0.0f);
    registry.emplace<BodyMass>(root, 10.0f);

    physics_system::Tick(MakeContext(world, intents, content, 1.0f));

    const auto& prev = registry.get<PreviousTransform>(root);
    CHECK(prev.position.x == Approx(1.0f));
    CHECK(prev.position.y == Approx(2.0f));
    CHECK(prev.rotation == Approx(0.3f));
}

TEST_CASE("PhysicsSystem leaves a massless-thrust entity with no Propulsion undisturbed",
          "[physics]") {
    // A station has BodyMass and Velocity (RigFactory always adds them) but no Propulsion or
    // ThrustInput. It must not crash and must not drift.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{5.0f, 5.0f}, 0.0f);
    registry.emplace<Velocity>(root);
    registry.emplace<BodyMass>(root, 500.0f);

    CHECK_NOTHROW(physics_system::Tick(MakeContext(world, intents, content, 1.0f)));

    const auto& transform = registry.get<WorldTransform>(root);
    CHECK(transform.position.x == Approx(5.0f));
    CHECK(transform.position.y == Approx(5.0f));
}
