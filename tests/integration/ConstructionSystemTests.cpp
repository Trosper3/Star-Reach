#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/registries/ContentLibrary.h"
#include "modes/space/systems/ConstructionSystem.h"
#include "modes/space/systems/PlayerRecordSystem.h"
#include "shared/components/Construction.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"

using sr::BuildStationRequest;
using sr::CargoHold;
using sr::FactionId;
using sr::FactionRef;
using sr::PlaceShipRequest;
using sr::Rig;
using sr::Wallet;
using sr::core::ContentLibrary;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace construction_system = sr::space::construction_system;
namespace player_record_system = sr::space::player_record_system;

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

}  // namespace

TEST_CASE("An affordable BuildStationRequest spends its cost exactly once and instantiates",
          "[construction]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity requester = registry.create();
    registry.emplace<Wallet>(requester, 1000);
    BuildStationRequest request;
    request.blueprint = sr::BlueprintId("aegis_outpost");
    request.cost = 500;
    registry.emplace<BuildStationRequest>(requester, request);

    const std::size_t before = registry.view<Rig>().size();
    construction_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<BuildStationRequest>(requester));
    CHECK(registry.get<Wallet>(requester).credits == 500);
    CHECK(registry.view<Rig>().size() > before);
}

TEST_CASE("A BuildStationRequest is refused when the requester cannot afford it",
          "[construction]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity requester = registry.create();
    registry.emplace<Wallet>(requester, 100);
    BuildStationRequest request;
    request.blueprint = sr::BlueprintId("aegis_outpost");
    request.cost = 500;
    registry.emplace<BuildStationRequest>(requester, request);

    const std::size_t before = registry.view<Rig>().size();
    construction_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Wallet>(requester).credits == 100);
    CHECK(registry.view<Rig>().size() == before);
}

TEST_CASE("A BuildStationRequest for a mobile (ship) blueprint is refused and not charged",
          "[construction]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity requester = registry.create();
    registry.emplace<Wallet>(requester, 1000);
    BuildStationRequest request;
    request.blueprint = sr::BlueprintId("aegis_vanguard");  // mobile: true -- not a station
    request.cost = 500;
    registry.emplace<BuildStationRequest>(requester, request);

    construction_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Wallet>(requester).credits == 1000);
}

TEST_CASE("An affordable PlaceShipRequest spends its cost exactly once and instantiates",
          "[construction]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity requester = registry.create();
    registry.emplace<Wallet>(requester, 1000);
    PlaceShipRequest request;
    request.blueprint = sr::BlueprintId("aegis_vanguard");
    request.cost = 300;
    registry.emplace<PlaceShipRequest>(requester, request);

    const std::size_t before = registry.view<Rig>().size();
    construction_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<PlaceShipRequest>(requester));
    CHECK(registry.get<Wallet>(requester).credits == 700);
    CHECK(registry.view<Rig>().size() > before);
}

TEST_CASE(
    "A BuildStationRequest attributes to the player's record faction, not the docked-at "
    "requester's own",
    "[construction]") {
    // architecture.md 12.30.3, amending 12.30.1: the requester is the rig root the request was
    // set on, which is the station itself while docked at a foreign one -- ConstructionSystem
    // must not stamp the built station under the host's flag.
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity foreignStation = registry.create();
    registry.emplace<Wallet>(foreignStation, 1000);
    registry.emplace<FactionRef>(foreignStation, FactionId("zenith"));
    player_record_system::SetFaction(registry, FactionId("aegis"));

    BuildStationRequest request;
    request.blueprint = sr::BlueprintId("aegis_outpost");
    request.cost = 500;
    registry.emplace<BuildStationRequest>(foreignStation, request);

    construction_system::Tick(MakeContext(world, intents, content));

    entt::entity built = entt::null;
    for (const entt::entity entity : registry.view<Rig>()) {
        built = entity;
    }
    REQUIRE((built != entt::null));
    CHECK(registry.get<FactionRef>(built).id == FactionId("aegis"));
}
