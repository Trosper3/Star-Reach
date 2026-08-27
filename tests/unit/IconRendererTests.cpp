#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/render/IconRenderer.h"
#include "modes/space/render/WorldRenderer.h"
#include "shared/components/Identity.h"
#include "shared/components/Targeting.h"

using Catch::Approx;
using sr::AimPoint;
using sr::PlayerLocation;
using sr::Vec2;
using sr::space::render::AimPointWorldPosition;
using sr::space::render::CameraView;
using sr::space::render::IsBodyCulled;
using sr::space::render::NeedsIconSubstitution;

TEST_CASE("AimPointWorldPosition returns nullopt with no PlayerLocation entity",
          "[icon-renderer]") {
    entt::registry registry;
    CHECK_FALSE(AimPointWorldPosition(registry).has_value());
}

TEST_CASE("AimPointWorldPosition returns nullopt when the player has no AimPoint yet",
          "[icon-renderer]") {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<PlayerLocation>(player, PlayerLocation{player});
    CHECK_FALSE(AimPointWorldPosition(registry).has_value());
}

TEST_CASE("AimPointWorldPosition returns the PlayerLocation entity's AimPoint", "[icon-renderer]") {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<PlayerLocation>(player, PlayerLocation{player});
    registry.emplace<AimPoint>(player, Vec2{200.0f, 50.0f});

    const auto result = AimPointWorldPosition(registry);
    REQUIRE(result.has_value());
    CHECK(result->x == Approx(200.0f));
    CHECK(result->y == Approx(50.0f));
}

// features.md 9.1: "objects outside the camera are simulated but not drawn." A 1000x800 viewport
// centred on the world origin at zoom 1 sees x in [-500, 500] and y in [-400, 400].
TEST_CASE("IsBodyCulled is false for a body inside the camera's view", "[icon-renderer]") {
    const CameraView camera{Vec2{0.0f, 0.0f}, 1.0f};
    CHECK_FALSE(IsBodyCulled(Vec2{100.0f, 50.0f}, 10.0f, camera, 1000.0f, 800.0f));
}

TEST_CASE("IsBodyCulled is true for a body well outside the camera's view", "[icon-renderer]") {
    const CameraView camera{Vec2{0.0f, 0.0f}, 1.0f};
    CHECK(IsBodyCulled(Vec2{5000.0f, 5000.0f}, 10.0f, camera, 1000.0f, 800.0f));
}

TEST_CASE("IsBodyCulled counts the body's radius, not just its centre", "[icon-renderer]") {
    const CameraView camera{Vec2{0.0f, 0.0f}, 1.0f};
    // Centre sits 40 units past the right edge (x = 500); a radius-50 body still overlaps it.
    CHECK_FALSE(IsBodyCulled(Vec2{540.0f, 0.0f}, 50.0f, camera, 1000.0f, 800.0f));
    // A radius-10 body at the same centre does not reach back to the edge.
    CHECK(IsBodyCulled(Vec2{540.0f, 0.0f}, 10.0f, camera, 1000.0f, 800.0f));
}

TEST_CASE("IsBodyCulled follows the camera target and zoom, not just screen size",
          "[icon-renderer]") {
    // Panned right and zoomed in 2x halves the world-space half-extent to 250.
    const CameraView camera{Vec2{1000.0f, 0.0f}, 2.0f};
    CHECK_FALSE(IsBodyCulled(Vec2{1000.0f, 0.0f}, 10.0f, camera, 1000.0f, 800.0f));
    CHECK(IsBodyCulled(Vec2{0.0f, 0.0f}, 10.0f, camera, 1000.0f, 800.0f));
}

// architecture.md's `BodyKind` comment: IconRenderer substitutes "when the body shrinks below a
// few pixels on zoom-out."
TEST_CASE("NeedsIconSubstitution is false for a body large enough to read at true scale",
          "[icon-renderer]") {
    CHECK_FALSE(NeedsIconSubstitution(10.0f, 1.0f));
}

TEST_CASE("NeedsIconSubstitution is true once the on-screen radius drops under the threshold",
          "[icon-renderer]") {
    // 10 world-unit radius at 0.1 zoom draws 1 screen pixel -- unreadable.
    CHECK(NeedsIconSubstitution(10.0f, 0.1f));
}

TEST_CASE("NeedsIconSubstitution reacts to zoom, not just world radius", "[icon-renderer]") {
    // The same body is legible zoomed in and needs substitution zoomed out.
    CHECK_FALSE(NeedsIconSubstitution(5.0f, 1.0f));
    CHECK(NeedsIconSubstitution(5.0f, 0.5f));
}
