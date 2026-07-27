#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/WarpSystem.h"
#include "shared/components/Physics.h"
#include "shared/components/Transform.h"
#include "shared/components/Warp.h"

using Catch::Approx;
using sr::PreviousTransform;
using sr::Vec2;
using sr::Velocity;
using sr::WarpRequest;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace warp_system = sr::space::warp_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

}  // namespace

TEST_CASE("WarpSystem teleports a rig's WorldTransform to the requested position", "[warp]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity ship = registry.create();
    registry.emplace<WorldTransform>(ship, Vec2{0.0f, 0.0f}, 1.2f);
    registry.emplace<WarpRequest>(ship, Vec2{5000.0f, -2000.0f});

    warp_system::Tick(MakeContext(world, intents, content));

    const WorldTransform& xf = registry.get<WorldTransform>(ship);
    CHECK(xf.position.x == Approx(5000.0f));
    CHECK(xf.position.y == Approx(-2000.0f));
    // Rotation is untouched -- a local warp repositions, it does not reorient.
    CHECK(xf.rotation == Approx(1.2f));
}

TEST_CASE("WarpSystem clears the request once consumed", "[warp]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity ship = registry.create();
    registry.emplace<WorldTransform>(ship, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<WarpRequest>(ship, Vec2{100.0f, 0.0f});

    warp_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<WarpRequest>(ship));
}

TEST_CASE(
    "WarpSystem snaps PreviousTransform to the destination so rendering does not "
    "interpolate a streak across the map",
    "[warp]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity ship = registry.create();
    registry.emplace<WorldTransform>(ship, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<PreviousTransform>(ship, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<WarpRequest>(ship, Vec2{3000.0f, 4000.0f});

    warp_system::Tick(MakeContext(world, intents, content));

    const PreviousTransform& prev = registry.get<PreviousTransform>(ship);
    CHECK(prev.position.x == Approx(3000.0f));
    CHECK(prev.position.y == Approx(4000.0f));
}

TEST_CASE("WarpSystem zeroes pre-warp Velocity", "[warp]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity ship = registry.create();
    registry.emplace<WorldTransform>(ship, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Velocity>(ship, Vec2{500.0f, 200.0f}, 4.0f);
    registry.emplace<WarpRequest>(ship, Vec2{100.0f, 0.0f});

    warp_system::Tick(MakeContext(world, intents, content));

    const Velocity& velocity = registry.get<Velocity>(ship);
    CHECK(velocity.linear.x == 0.0f);
    CHECK(velocity.linear.y == 0.0f);
    CHECK(velocity.angular == 0.0f);
}

TEST_CASE("WarpSystem leaves a rig with no WarpRequest untouched", "[warp]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity ship = registry.create();
    registry.emplace<WorldTransform>(ship, Vec2{42.0f, 7.0f}, 0.0f);

    warp_system::Tick(MakeContext(world, intents, content));

    const WorldTransform& xf = registry.get<WorldTransform>(ship);
    CHECK(xf.position.x == Approx(42.0f));
    CHECK(xf.position.y == Approx(7.0f));
}
