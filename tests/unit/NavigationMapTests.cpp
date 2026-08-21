#include <catch2/catch_test_macros.hpp>

#include <utility>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/knowledge/KnowledgeNetwork.h"
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
using sr::core::knowledge::FactionNetworkId;
using sr::core::knowledge::KnowledgeNetwork;
using sr::core::knowledge::KnowledgeStore;
using sr::core::knowledge::NetworkEntryKind;
using sr::core::knowledge::NetworkOwnerKind;
namespace navigation_map = sr::space::ui::navigation_map;

namespace {

// Mirrors core/knowledge/KnowledgeSeeding.h's SeedFactionNetworks for one faction -- every
// KnowledgeStore reader below fails closed against a network nothing registered, the same
// convention DiscoverySystemTests.cpp uses.
void SeedFaction(KnowledgeStore& knowledge, const FactionId& faction) {
    KnowledgeNetwork network;
    network.ownerKind = NetworkOwnerKind::Faction;
    knowledge.LoadNetwork(FactionNetworkId(faction), std::move(network));
}

}  // namespace

TEST_CASE("DiscoveredSystemIds returns a faction's discovered systems, sorted",
          "[navigation-map]") {
    KnowledgeStore knowledge;
    SeedFaction(knowledge, FactionId("aegis"));
    knowledge.Grant(FactionNetworkId(FactionId("aegis")), NetworkEntryKind::DiscoveredSystem,
                    "kepler");
    knowledge.Grant(FactionNetworkId(FactionId("aegis")), NetworkEntryKind::DiscoveredSystem,
                    "sol");

    const std::vector<std::string> ids =
        navigation_map::DiscoveredSystemIds(FactionId("aegis"), knowledge);

    REQUIRE(ids.size() == 2);
    CHECK(ids[0] == "kepler");
    CHECK(ids[1] == "sol");
}

TEST_CASE("DiscoveredSystemIds is empty for a faction with no discoveries", "[navigation-map]") {
    KnowledgeStore knowledge;
    SeedFaction(knowledge, FactionId("aegis"));
    knowledge.Grant(FactionNetworkId(FactionId("aegis")), NetworkEntryKind::DiscoveredSystem,
                    "sol");

    CHECK(navigation_map::DiscoveredSystemIds(FactionId("reavers"), knowledge).empty());
}

TEST_CASE("DiscoveredSystemIds is empty for a faction with no registered network",
          "[navigation-map]") {
    KnowledgeStore knowledge;  // "aegis" never seeded.

    CHECK(navigation_map::DiscoveredSystemIds(FactionId("aegis"), knowledge).empty());
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

KnowledgeStore MakeDiscoveredStore() {
    KnowledgeStore knowledge;
    SeedFaction(knowledge, FactionId("aegis"));
    knowledge.Grant(FactionNetworkId(FactionId("aegis")), NetworkEntryKind::DiscoveredSystem,
                    kSystemId);
    return knowledge;
}

}  // namespace

TEST_CASE("VisibleHostileRigs is empty when the player has no sensor data", "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = registry.create();
    DiplomacyMatrix diplomacy;
    KnowledgeStore knowledge = MakeDiscoveredStore();

    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &knowledge, kSystemId)
              .empty());
}

TEST_CASE("VisibleHostileRigs includes an in-range Hostile-relation rig", "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    const entt::entity hostile = MakeHostile(registry, Vec2{50.0f, 0.0f}, "reavers");
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    KnowledgeStore knowledge = MakeDiscoveredStore();

    const auto visible =
        navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &knowledge, kSystemId);
    REQUIRE(visible.size() == 1);
    CHECK(visible.front() == hostile);
}

TEST_CASE("VisibleHostileRigs excludes an out-of-range rig", "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    MakeHostile(registry, Vec2{500.0f, 0.0f}, "reavers");
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    KnowledgeStore knowledge = MakeDiscoveredStore();

    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &knowledge, kSystemId)
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
    KnowledgeStore knowledge = MakeDiscoveredStore();

    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &knowledge, kSystemId)
              .empty());
}

TEST_CASE("VisibleHostileRigs excludes a same-faction rig", "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    MakeHostile(registry, Vec2{10.0f, 0.0f}, "aegis");
    DiplomacyMatrix diplomacy;
    KnowledgeStore knowledge = MakeDiscoveredStore();

    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &knowledge, kSystemId)
              .empty());
}

TEST_CASE("VisibleHostileRigs excludes everything when the system is not discovered",
          "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    MakeHostile(registry, Vec2{10.0f, 0.0f}, "reavers");
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    KnowledgeStore knowledge;
    SeedFaction(knowledge, FactionId("aegis"));  // "sol" never granted.

    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &knowledge, kSystemId)
              .empty());
}

TEST_CASE("VisibleHostileRigs excludes everything when the faction has no registered network",
          "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    MakeHostile(registry, Vec2{10.0f, 0.0f}, "reavers");
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    KnowledgeStore knowledge;  // "aegis" never seeded at all.

    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &knowledge, kSystemId)
              .empty());
}

TEST_CASE("VisibleHostileRigs fails closed with no diplomacy or knowledge pointer wired",
          "[navigation-map]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 100.0f);
    MakeHostile(registry, Vec2{10.0f, 0.0f}, "reavers");
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    KnowledgeStore knowledge = MakeDiscoveredStore();

    CHECK(navigation_map::VisibleHostileRigs(registry, player, nullptr, &knowledge, kSystemId)
              .empty());
    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, nullptr, kSystemId)
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
    KnowledgeStore knowledge = MakeDiscoveredStore();

    CHECK(navigation_map::VisibleHostileRigs(registry, player, &diplomacy, &knowledge, kSystemId)
              .empty());
}
