#include <cmath>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/OrbitSystem.h"
#include "shared/components/Orbit.h"
#include "shared/components/Physics.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"
#include "shared/math/Vec2.h"

using Catch::Approx;
using sr::GravityWell;
using sr::OrbitBody;
using sr::PreviousTransform;
using sr::Vec2;
using sr::Velocity;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace orbit_system = sr::space::orbit_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt,
                          unsigned long long tick) {
    return SystemContext{world, intents, content, dt, tick};
}

}  // namespace

TEST_CASE("OrbitSystem places a body at phase zero, radius from the origin", "[orbit]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity planet = registry.create();
    registry.emplace<OrbitBody>(planet, entt::null, 100.0f, 0.0f, 0.0f);
    registry.emplace<WorldTransform>(planet);

    orbit_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f, 0));

    const auto& transform = registry.get<WorldTransform>(planet);
    CHECK(transform.position.x == Approx(100.0f));
    CHECK(transform.position.y == Approx(0.0f).margin(0.0001f));
}

TEST_CASE("OrbitSystem's position is a pure function of tick count, not an accumulator",
          "[orbit]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity planet = registry.create();
    registry.emplace<OrbitBody>(planet, entt::null, 10.0f, 1.0f, 0.0f);
    registry.emplace<WorldTransform>(planet);

    // Same tick evaluated twice must produce the identical position -- a fast-forward that
    // evaluates tick 300 directly must agree with one that stepped through 1..300 one at a time.
    orbit_system::Tick(MakeContext(world, intents, content, 1.0f, 5));
    const Vec2 first = registry.get<WorldTransform>(planet).position;

    orbit_system::Tick(MakeContext(world, intents, content, 1.0f, 5));
    const Vec2 second = registry.get<WorldTransform>(planet).position;

    CHECK(first.x == Approx(second.x));
    CHECK(first.y == Approx(second.y));

    const float expectedAngle = 5.0f;  // phase(0) + angularSpeed(1) * elapsed(tick 5 * dt 1)
    const float radius = 10.0f;
    CHECK(first.x == Approx(radius * std::cos(expectedAngle)).margin(0.0001f));
    CHECK(first.y == Approx(radius * std::sin(expectedAngle)).margin(0.0001f));
}

TEST_CASE("OrbitSystem writes PreviousTransform to the pre-move position when present", "[orbit]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity planet = registry.create();
    registry.emplace<OrbitBody>(planet, entt::null, 100.0f, 1.0f, 0.0f);
    registry.emplace<WorldTransform>(planet, Vec2{100.0f, 0.0f}, 0.0f);
    registry.emplace<PreviousTransform>(planet, Vec2{100.0f, 0.0f}, 0.0f);

    orbit_system::Tick(MakeContext(world, intents, content, 1.0f, 1));

    const auto& prev = registry.get<PreviousTransform>(planet);
    CHECK(prev.position.x == Approx(100.0f));
    CHECK(prev.position.y == Approx(0.0f).margin(0.0001f));
    const auto& current = registry.get<WorldTransform>(planet);
    CHECK(current.position.y != Approx(prev.position.y).margin(0.0001f));
}

TEST_CASE("OrbitSystem does not add PreviousTransform to a body that lacks one", "[orbit]") {
    // A drop or wreck (WorldRenderer.h) deliberately carries no PreviousTransform to satisfy this
    // loop; OrbitSystem must not force-emplace one onto an orbiting body that omits it either.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity planet = registry.create();
    registry.emplace<OrbitBody>(planet, entt::null, 100.0f, 1.0f, 0.0f);
    registry.emplace<WorldTransform>(planet, Vec2{100.0f, 0.0f}, 0.0f);

    orbit_system::Tick(MakeContext(world, intents, content, 1.0f, 0));

    CHECK_FALSE(registry.all_of<PreviousTransform>(planet));
}

TEST_CASE("OrbitSystem resolves a moon's position relative to its orbiting center", "[orbit]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity planet = registry.create();
    registry.emplace<OrbitBody>(planet, entt::null, 50.0f, 0.0f, 0.0f);
    registry.emplace<WorldTransform>(planet);

    const entt::entity moon = registry.create();
    registry.emplace<OrbitBody>(moon, planet, 10.0f, 0.0f, sr::kPi / 2.0f);
    registry.emplace<WorldTransform>(moon);

    orbit_system::Tick(MakeContext(world, intents, content, 1.0f, 0));

    const auto& planetTransform = registry.get<WorldTransform>(planet);
    CHECK(planetTransform.position.x == Approx(50.0f));

    const auto& moonTransform = registry.get<WorldTransform>(moon);
    CHECK(moonTransform.position.x == Approx(50.0f).margin(0.0001f));
    CHECK(moonTransform.position.y == Approx(10.0f));
}

TEST_CASE("OrbitSystem pulls a ship's velocity toward a nearby gravity well", "[orbit]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity sun = registry.create();
    registry.emplace<WorldTransform>(sun, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<GravityWell>(sun, 100.0f, 10.0f);

    const entt::entity ship = registry.create();
    registry.emplace<WorldTransform>(ship, Vec2{50.0f, 0.0f}, 0.0f);
    registry.emplace<Velocity>(ship);

    orbit_system::Tick(MakeContext(world, intents, content, 1.0f, 0));

    // distance 50 of range 100 -> falloff 0.5, accel = strength(10) * 0.5^2 = 2.5, pulling -x.
    const auto& velocity = registry.get<Velocity>(ship);
    CHECK(velocity.linear.x == Approx(-2.5f));
    CHECK(velocity.linear.y == Approx(0.0f).margin(0.0001f));
}

TEST_CASE("OrbitSystem leaves a ship's velocity untouched outside a gravity well's range",
          "[orbit]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity sun = registry.create();
    registry.emplace<WorldTransform>(sun, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<GravityWell>(sun, 100.0f, 10.0f);

    const entt::entity ship = registry.create();
    registry.emplace<WorldTransform>(ship, Vec2{150.0f, 0.0f}, 0.0f);
    registry.emplace<Velocity>(ship);

    orbit_system::Tick(MakeContext(world, intents, content, 1.0f, 0));

    const auto& velocity = registry.get<Velocity>(ship);
    CHECK(velocity.linear.x == Approx(0.0f).margin(0.0001f));
    CHECK(velocity.linear.y == Approx(0.0f).margin(0.0001f));
}

TEST_CASE("OrbitSystem does not apply gravity-well pull to a body whose position is orbit-driven",
          "[orbit]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity sun = registry.create();
    registry.emplace<WorldTransform>(sun, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<GravityWell>(sun, 100.0f, 10.0f);

    // An orbiting body that (unusually) also carries a Velocity -- its position is already fully
    // determined by OrbitBody, so the well must not perturb it.
    const entt::entity planet = registry.create();
    registry.emplace<OrbitBody>(planet, entt::null, 50.0f, 0.0f, 0.0f);
    registry.emplace<WorldTransform>(planet);
    registry.emplace<Velocity>(planet);

    orbit_system::Tick(MakeContext(world, intents, content, 1.0f, 0));

    const auto& velocity = registry.get<Velocity>(planet);
    CHECK(velocity.linear.x == Approx(0.0f).margin(0.0001f));
    CHECK(velocity.linear.y == Approx(0.0f).margin(0.0001f));
}
