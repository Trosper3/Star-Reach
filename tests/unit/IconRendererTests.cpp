#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/render/IconRenderer.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

using Catch::Approx;
using sr::Destroyed;
using sr::PlayerControlled;
using sr::PreviousTransform;
using sr::Target;
using sr::Vec2;
using sr::WorldTransform;
using sr::space::render::TargetWorldPosition;

TEST_CASE("TargetWorldPosition returns nullopt with no PlayerControlled entity",
          "[icon-renderer]") {
    entt::registry registry;
    CHECK_FALSE(TargetWorldPosition(registry, 0.0f).has_value());
}

TEST_CASE("TargetWorldPosition returns nullopt when the player has no Target", "[icon-renderer]") {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<PlayerControlled>(player);
    CHECK_FALSE(TargetWorldPosition(registry, 0.0f).has_value());
}

TEST_CASE("TargetWorldPosition returns nullopt when Target.rig is unset", "[icon-renderer]") {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<PlayerControlled>(player);
    registry.emplace<Target>(player);  // Default-constructed: rig == entt::null.
    CHECK_FALSE(TargetWorldPosition(registry, 0.0f).has_value());
}

TEST_CASE("TargetWorldPosition returns nullopt for a Destroyed target", "[icon-renderer]") {
    entt::registry registry;
    const entt::entity enemy = registry.create();
    registry.emplace<WorldTransform>(enemy, Vec2{100.0f, 0.0f}, 0.0f);
    registry.emplace<PreviousTransform>(enemy, Vec2{100.0f, 0.0f}, 0.0f);
    registry.emplace<Destroyed>(enemy);

    const entt::entity player = registry.create();
    registry.emplace<PlayerControlled>(player);
    registry.emplace<Target>(player, enemy, entt::null);

    CHECK_FALSE(TargetWorldPosition(registry, 0.0f).has_value());
}

TEST_CASE("TargetWorldPosition returns the target's current position at alpha=1",
          "[icon-renderer]") {
    entt::registry registry;
    const entt::entity enemy = registry.create();
    registry.emplace<WorldTransform>(enemy, Vec2{200.0f, 50.0f}, 0.0f);
    registry.emplace<PreviousTransform>(enemy, Vec2{100.0f, 0.0f}, 0.0f);

    const entt::entity player = registry.create();
    registry.emplace<PlayerControlled>(player);
    registry.emplace<Target>(player, enemy, entt::null);

    const auto result = TargetWorldPosition(registry, 1.0f);
    REQUIRE(result.has_value());
    CHECK(result->x == Approx(200.0f));
    CHECK(result->y == Approx(50.0f));
}

TEST_CASE("TargetWorldPosition interpolates between previous and current position",
          "[icon-renderer]") {
    entt::registry registry;
    const entt::entity enemy = registry.create();
    registry.emplace<WorldTransform>(enemy, Vec2{100.0f, 0.0f}, 0.0f);
    registry.emplace<PreviousTransform>(enemy, Vec2{0.0f, 0.0f}, 0.0f);

    const entt::entity player = registry.create();
    registry.emplace<PlayerControlled>(player);
    registry.emplace<Target>(player, enemy, entt::null);

    const auto result = TargetWorldPosition(registry, 0.5f);
    REQUIRE(result.has_value());
    CHECK(result->x == Approx(50.0f));
}
