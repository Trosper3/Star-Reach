#include <catch2/catch_test_macros.hpp>

#include "core/diplomacy/DiplomacyMatrix.h"
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
using sr::core::diplomacy::DiplomacyMatrix;
using sr::core::diplomacy::Relation;
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

namespace {

// "sol" throughout -- the resident system id, discovered by "aegis" so the fog gate passes.
constexpr const char* kSystemId = "sol";

DiscoveryState MakeDiscoveredState() {
    DiscoveryState discovery;
    discovery.Discover(FactionId("aegis"), kSystemId);
    return discovery;
}

}  // namespace

TEST_CASE("VisibleHostileRigs is empty when the player has no sensor data", "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = registry.create();
    DiplomacyMatrix diplomacy;
    DiscoveryState discovery = MakeDiscoveredState();

    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &discovery, kSystemId)
              .empty());
}

TEST_CASE("VisibleHostileRigs includes an in-range Hostile-relation rig", "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    const entt::entity hostile = MakeHostile(registry, Vec2{50.0f, 0.0f}, "reavers");
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    DiscoveryState discovery = MakeDiscoveredState();

    const auto visible =
        navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &discovery, kSystemId);
    REQUIRE(visible.size() == 1);
    CHECK(visible.front() == hostile);
}

TEST_CASE("VisibleHostileRigs excludes an out-of-range rig", "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    MakeHostile(registry, Vec2{500.0f, 0.0f}, "reavers");
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    DiscoveryState discovery = MakeDiscoveredState();

    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &discovery, kSystemId)
              .empty());
}

TEST_CASE("VisibleHostileRigs excludes a Neutral-relation rig", "[navigation-map]") {
    // features.md 5.3 / architecture.md 15.1 finding 17: a different FactionId alone is no
    // longer "hostile" -- only the Hostile and War bands are, the same threshold finding N sets
    // for TargetingSystem. "aegis" vs "reavers" is left unset here -- Relation::Neutral.
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    MakeHostile(registry, Vec2{10.0f, 0.0f}, "reavers");
    DiplomacyMatrix diplomacy;
    DiscoveryState discovery = MakeDiscoveredState();

    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &discovery, kSystemId)
              .empty());
}

TEST_CASE("VisibleHostileRigs excludes a same-faction rig", "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    MakeHostile(registry, Vec2{10.0f, 0.0f}, "aegis");
    DiplomacyMatrix diplomacy;
    DiscoveryState discovery = MakeDiscoveredState();

    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &discovery, kSystemId)
              .empty());
}

TEST_CASE("VisibleHostileRigs excludes everything when the system is not discovered",
          "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    MakeHostile(registry, Vec2{10.0f, 0.0f}, "reavers");
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    DiscoveryState discovery;  // "sol" never discovered by "aegis".

    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &discovery, kSystemId)
              .empty());
}

TEST_CASE("VisibleHostileRigs fails closed with no diplomacy or discovery pointer wired",
          "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    MakeHostile(registry, Vec2{10.0f, 0.0f}, "reavers");
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    DiscoveryState discovery = MakeDiscoveredState();

    CHECK(
        navigation_map::VisibleHostileRigs(registry, player, nullptr, &discovery, kSystemId)
            .empty());
    CHECK(
        navigation_map::VisibleHostileRigs(registry, player, &diplomacy, nullptr, kSystemId)
            .empty());
}

TEST_CASE("VisibleHostileRigs excludes a non-Targetable rig in range", "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    const entt::entity rig = registry.create();
    registry.emplace<FactionRef>(rig, FactionId("reavers"));
    registry.emplace<WorldTransform>(rig, Vec2{10.0f, 0.0f}, 0.0f);
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    DiscoveryState discovery = MakeDiscoveredState();

    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &discovery, kSystemId)
              .empty());
}
