#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>
#include <string>
#include <utility>
#include <vector>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/ui/SensorContacts.h"
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
using sr::space::ui::sensor_contacts::Build;
using sr::space::ui::sensor_contacts::ClampToEdge;
using sr::space::ui::sensor_contacts::Contact;

namespace {

constexpr const char* kSystemId = "sol";

KnowledgeStore MakeDiscoveredStore() {
    KnowledgeStore knowledge;
    KnowledgeNetwork network;
    network.ownerKind = NetworkOwnerKind::Faction;
    knowledge.LoadNetwork(FactionNetworkId(FactionId("aegis")), std::move(network));
    knowledge.Grant(FactionNetworkId(FactionId("aegis")), NetworkEntryKind::DiscoveredSystem,
                    kSystemId);
    return knowledge;
}

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

TEST_CASE("Build returns a contact for an in-range hostile rig", "[sensor-contacts]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 1000.0f);
    const entt::entity hostile = MakeHostile(registry, Vec2{300.0f, 0.0f}, "reavers");
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    KnowledgeStore knowledge = MakeDiscoveredStore();

    const std::vector<Contact> contacts =
        Build(registry, player, &diplomacy, &knowledge, kSystemId);
    REQUIRE(contacts.size() == 1);
    CHECK(contacts.front().rig == hostile);
    CHECK(contacts.front().worldPosition.x == 300.0f);
}

TEST_CASE("Build is empty once SensorRange is zeroed -- the destroyed-sensor case",
          "[sensor-contacts]") {
    // Issue #234's own test: destroying the sensor module disables the contact indicators.
    // shared/rig/ModuleAttachment.cpp's RecomputeRigTotals zeroes SensorRange::units rather than
    // removing the component, so this exercises the same live state a real destroyed hardpoint
    // leaves behind.
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 0.0f);
    MakeHostile(registry, Vec2{10.0f, 0.0f}, "reavers");
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);
    KnowledgeStore knowledge = MakeDiscoveredStore();

    CHECK(Build(registry, player, &diplomacy, &knowledge, kSystemId).empty());
}

TEST_CASE("Build is empty with no diplomacy or knowledge pointer wired", "[sensor-contacts]") {
    entt::registry registry;
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f}, 1000.0f);
    MakeHostile(registry, Vec2{10.0f, 0.0f}, "reavers");
    KnowledgeStore knowledge = MakeDiscoveredStore();
    DiplomacyMatrix diplomacy;

    CHECK(Build(registry, player, nullptr, &knowledge, kSystemId).empty());
    CHECK(Build(registry, player, &diplomacy, nullptr, kSystemId).empty());
}

TEST_CASE("ClampToEdge leaves an on-screen point untouched and marks it inside",
          "[sensor-contacts]") {
    const auto result =
        ClampToEdge(Vec2{400.0f, 300.0f}, Vec2{400.0f, 300.0f}, 800.0f, 600.0f, 20.0f);
    CHECK(result.pointWasInside);
    CHECK(result.position.x == 400.0f);
    CHECK(result.position.y == 300.0f);
}

TEST_CASE("ClampToEdge pulls an off-screen point onto the right edge", "[sensor-contacts]") {
    // Directly right of center, well past the viewport -- should land exactly on the inset right
    // edge at the same height as the center.
    const auto result =
        ClampToEdge(Vec2{5000.0f, 300.0f}, Vec2{400.0f, 300.0f}, 800.0f, 600.0f, 20.0f);
    CHECK_FALSE(result.pointWasInside);
    CHECK(result.position.x == Catch::Approx(780.0f));
    CHECK(result.position.y == Catch::Approx(300.0f));
}

TEST_CASE("ClampToEdge pulls a diagonally off-screen point onto whichever edge it reaches first",
          "[sensor-contacts]") {
    // Up and to the right of center, off both edges. The top edge (dy) is reached at a smaller t
    // than the right edge (dx) here, so the result lands exactly on the top edge, part way across
    // rather than pinned to a corner.
    const auto result =
        ClampToEdge(Vec2{5000.0f, -5000.0f}, Vec2{400.0f, 300.0f}, 800.0f, 600.0f, 20.0f);
    CHECK_FALSE(result.pointWasInside);
    CHECK(result.position.y == Catch::Approx(20.0f));
    CHECK(result.position.x > 400.0f);
    CHECK(result.position.x < 780.0f);
}

TEST_CASE("ClampToEdge stays within the inset rectangle bounds", "[sensor-contacts]") {
    const auto result =
        ClampToEdge(Vec2{-9000.0f, 50.0f}, Vec2{400.0f, 300.0f}, 800.0f, 600.0f, 20.0f);
    CHECK_FALSE(result.pointWasInside);
    CHECK(result.position.x >= 20.0f);
    CHECK(result.position.x <= 780.0f);
    CHECK(result.position.y >= 20.0f);
    CHECK(result.position.y <= 580.0f);
}
