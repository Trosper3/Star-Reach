#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/render/IconRenderer.h"
#include "shared/components/Identity.h"
#include "shared/components/Targeting.h"

using Catch::Approx;
using sr::AimPoint;
using sr::PlayerControlled;
using sr::Vec2;
using sr::space::render::AimPointWorldPosition;

TEST_CASE("AimPointWorldPosition returns nullopt with no PlayerControlled entity",
          "[icon-renderer]") {
    entt::registry registry;
    CHECK_FALSE(AimPointWorldPosition(registry).has_value());
}

TEST_CASE("AimPointWorldPosition returns nullopt when the player has no AimPoint yet",
          "[icon-renderer]") {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<PlayerControlled>(player);
    CHECK_FALSE(AimPointWorldPosition(registry).has_value());
}

TEST_CASE("AimPointWorldPosition returns the PlayerControlled entity's AimPoint",
          "[icon-renderer]") {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<PlayerControlled>(player);
    registry.emplace<AimPoint>(player, Vec2{200.0f, 50.0f});

    const auto result = AimPointWorldPosition(registry);
    REQUIRE(result.has_value());
    CHECK(result->x == Approx(200.0f));
    CHECK(result->y == Approx(50.0f));
}
