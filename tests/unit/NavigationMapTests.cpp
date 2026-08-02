#include <catch2/catch_test_macros.hpp>

#include "core/galaxy/Discovery.h"
#include "modes/space/ui/NavigationMap.h"
#include "shared/components/Identity.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"

using sr::FactionId;
using sr::FactionRef;
using sr::SensorRange;
using sr::Targetable;
using sr::Vec2;
using sr::WorldTransform;
using sr::core::galaxy::DiscoveryState;
namespace navigation_map = sr::space::ui::navigation_map;

TEST_CASE("DiscoveredSystemIds returns a faction's discovered systems, sorted",
          "[navigation-map]") {
    DiscoveryState discovery;
    discovery.Discover(FactionId("aegis"), "kepler");
    discovery.Discover(FactionId("aegis"), "sol");

    const std::vector<std::string> ids =
        navigation_map::DiscoveredSystemIds(FactionId("aegis"), discovery);

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == "kepler");
    CHECK(ids[1] == "sol");
}

TEST_CASE("DiscoveredSystemIds is empty for a faction with no discoveries", "[navigation-map]") {
    DiscoveryState discovery;
    discovery.Discover(FactionId("aegis"), "sol");

    CHECK(navigation_map::DiscoveredSystemIds(FactionId("reavers"), discovery).empty());
}

TEST_CASE("ShowsShipIcons is true only at System level", "[navigation-map]") {
    CHECK_FALSE(navigation_map::ShowsShipIcons(navigation_map::ZoomLevel::Galaxy));
    CHECK_FALSE(navigation_map::ShowsShipIcons(navigation_map::ZoomLevel::Region));
    CHECK(navigation_map::ShowsShipIcons(navigation_map::ZoomLevel::System));
    CHECK_FALSE(navigation_map::ShowsShipIcons(navigation_map::ZoomLevel::Tactical));
}

namespace {

entt::entity MakePlayer(entt::registry& registry, Vec2 position, float sensorRange) {
    const entt::entity player = registry.create();
    registry.emplace<FactionRef>(player, FactionId("aegis"));
    registry.emplace<WorldTransform>(player, position, 0.0f);
    registry.emplace<SensorRange>(player, sensorRange);
    return player;
}

entt::entity MakeHostile(entt::registry& registry, Vec2 position, const std::string& faction) {
    const entt::entity rig = registry.create();
    registry.emplace<FactionRef>(rig, FactionId(faction));
    registry.emplace<WorldTransform>(rig, position, 0.0f);
    registry.emplace<Targetable>(rig);
    return rig;
}

}  // namespace

TEST_CASE("VisibleHostileRigs is empty when the player has no sensor data", "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = registry.create();

    CHECK(navigation_map::VisibleHostileRigs(registry, player).empty());
}

TEST_CASE("VisibleHostileRigs includes an in-range hostile of a different faction",
          "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    const entt::entity hostile = MakeHostile(registry, Vec2{50.0f, 0.0f}, "reavers");

    const auto visible = navigation_map::VisibleHostileRigs(registry, player);
    REQUIRE(visible.size() == 1);
    CHECK(visible.front() == hostile);
}

TEST_CASE("VisibleHostileRigs excludes an out-of-range rig", "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    MakeHostile(registry, Vec2{500.0f, 0.0f}, "reavers");

    CHECK(navigation_map::VisibleHostileRigs(registry, player).empty());
}

TEST_CASE("VisibleHostileRigs excludes a same-faction rig", "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    MakeHostile(registry, Vec2{10.0f, 0.0f}, "aegis");

    CHECK(navigation_map::VisibleHostileRigs(registry, player).empty());
}

TEST_CASE("VisibleHostileRigs excludes a non-Targetable rig in range", "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    const entt::entity rig = registry.create();
    registry.emplace<FactionRef>(rig, FactionId("reavers"));
    registry.emplace<WorldTransform>(rig, Vec2{10.0f, 0.0f}, 0.0f);

    CHECK(navigation_map::VisibleHostileRigs(registry, player).empty());
}
