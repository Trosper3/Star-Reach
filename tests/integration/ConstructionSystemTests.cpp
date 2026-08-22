#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/events/Intent.h"
#include "core/events/IntentQueue.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/systems/ConstructionSystem.h"
#include "modes/space/systems/PlayerRecordSystem.h"
#include "modes/space/ui/CustomizeMenu.h"
#include "shared/blueprints/Validation.h"
#include "shared/components/Construction.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/NetworkOwner.h"
#include "shared/components/Rig.h"

using sr::BuildStationRequest;
using sr::CargoHold;
using sr::FactionId;
using sr::FactionRef;
using sr::KnowledgeNetworkId;
using sr::NetworkOwner;
using sr::PlaceShipRequest;
using sr::Rig;
using sr::ShipBlueprint;
using sr::ValidationRule;
using sr::Wallet;
using sr::core::ContentLibrary;
using sr::core::knowledge::KnowledgeStore;
using sr::core::knowledge::NetworkOwnerKind;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace construction_system = sr::space::construction_system;
namespace player_record_system = sr::space::player_record_system;
namespace customize_menu = sr::space::ui::customize_menu;

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

// Threads knowledge + a writable ContentLibrary through, the two pointers
// ConsumeSaveTemplateRequests and the knowledge gate both need -- `content` must outlive `ctx`
// and stay the same object passed as both the const `content` field and the non-const
// `craftedModules` pointer (SpaceFlight.cpp's own "craftedModules aliases content_ itself"
// pattern, architecture.md 12.24 step 6).
SystemContext MakeContextWithKnowledge(SystemWorld& world, const sr::core::IntentQueue& intents,
                                       ContentLibrary& content, KnowledgeStore& knowledge) {
    SystemContext ctx{world, intents, content, 1.0f / 60.0f, 0};
    ctx.knowledge = &knowledge;
    ctx.craftedModules = &content;
    return ctx;
}

// A guaranteed-to-validate draft: an existing authored ship blueprint, cloned under a new id.
// Building one from raw AddMount/EquipModule calls (as CustomizeMenuTests.cpp's own FakeLibrary
// does) means re-deriving Validation.h's full geometry ruleset by hand; cloning a design the real
// content set already ships proves the save->build chain without re-litigating that ruleset here.
ShipBlueprint MakeDraft(const ContentLibrary& content, const char* newId) {
    ShipBlueprint draft = *content.FindShip(sr::BlueprintId("aegis_vanguard"));
    draft.id = sr::BlueprintId(newId);
    return draft;
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

TEST_CASE("A saved Template retains its body and can be built", "[construction]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    KnowledgeStore knowledge;
    const KnowledgeNetworkId network = knowledge.Create(NetworkOwnerKind::Player);

    const ShipBlueprint draft = MakeDraft(content, "player_copy_ship");
    intents.Push(customize_menu::BuildSaveRequest(sr::ActorId{1}, network, draft));

    construction_system::Tick(MakeContextWithKnowledge(world, intents, content, knowledge));

    // Granted into the network, and the body -- not just the id -- is resolvable afterward.
    REQUIRE(knowledge.Get(network) != nullptr);
    CHECK(knowledge.Get(network)->savedTemplates.count("player_copy_ship") == 1);
    REQUIRE(content.FindShip(sr::BlueprintId("player_copy_ship")) != nullptr);
    CHECK(content.FindShip(sr::BlueprintId("player_copy_ship"))->rig.mounts.size() ==
          draft.rig.mounts.size());

    // Buildable: PlaceShipRequest for the saved id, from a requester whose network holds it.
    const entt::entity requester = registry.create();
    registry.emplace<Wallet>(requester, 1000);
    registry.emplace<NetworkOwner>(requester, network);
    PlaceShipRequest request;
    request.blueprint = sr::BlueprintId("player_copy_ship");
    request.cost = 300;
    registry.emplace<PlaceShipRequest>(requester, request);

    const std::size_t before = registry.view<Rig>().size();
    construction_system::Tick(MakeContextWithKnowledge(world, intents, content, knowledge));

    CHECK(registry.get<Wallet>(requester).credits == 700);
    CHECK(registry.view<Rig>().size() > before);
}

TEST_CASE("A PlaceShipRequest for a drafted Template is refused without network membership",
          "[construction]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    KnowledgeStore knowledge;
    const KnowledgeNetworkId ownerNetwork = knowledge.Create(NetworkOwnerKind::Player);
    const KnowledgeNetworkId strangerNetwork = knowledge.Create(NetworkOwnerKind::Player);

    const ShipBlueprint draft = MakeDraft(content, "guarded_ship");
    intents.Push(customize_menu::BuildSaveRequest(sr::ActorId{1}, ownerNetwork, draft));
    construction_system::Tick(MakeContextWithKnowledge(world, intents, content, knowledge));
    REQUIRE(content.IsDraftedTemplate(sr::BlueprintId("guarded_ship")));

    const entt::entity requester = registry.create();
    registry.emplace<Wallet>(requester, 1000);
    registry.emplace<NetworkOwner>(requester, strangerNetwork);  // Does not hold the design.
    PlaceShipRequest request;
    request.blueprint = sr::BlueprintId("guarded_ship");
    request.cost = 300;
    registry.emplace<PlaceShipRequest>(requester, request);

    construction_system::Tick(MakeContextWithKnowledge(world, intents, content, knowledge));

    CHECK_FALSE(registry.all_of<PlaceShipRequest>(requester));  // Refused, request cleared.
    CHECK(registry.get<Wallet>(requester).credits == 1000);     // Never charged.
}

TEST_CASE("A BuildStationRequest for a base-game blueprint needs no network membership",
          "[construction]") {
    // architecture.md 12.30.8: the knowledge gate applies only to a drafted Template -- authored
    // content stays buildable exactly as before this issue, and this is the existing "affordable
    // request succeeds" test with ctx.knowledge now non-null, proving the gate does not regress
    // the base-game path.
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    KnowledgeStore knowledge;

    const entt::entity requester = registry.create();
    registry.emplace<Wallet>(requester, 1000);
    BuildStationRequest request;
    request.blueprint = sr::BlueprintId("aegis_outpost");
    request.cost = 500;
    registry.emplace<BuildStationRequest>(requester, request);

    construction_system::Tick(MakeContextWithKnowledge(world, intents, content, knowledge));

    CHECK(registry.get<Wallet>(requester).credits == 500);
}

TEST_CASE("ConsumeSaveTemplateRequests refuses a draft naming an unknown module",
          "[construction]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    sr::core::IntentQueue intents;
    KnowledgeStore knowledge;
    const KnowledgeNetworkId network = knowledge.Create(NetworkOwnerKind::Player);

    ShipBlueprint draft = MakeDraft(content, "broken_ship");
    REQUIRE_FALSE(draft.rig.mounts.empty());
    draft.rig.mounts.front().modules.push_back(sr::ModuleId("definitely_not_a_module"));
    REQUIRE(Validate(draft, content).HasRule(ValidationRule::UnresolvedId));

    intents.Push(customize_menu::BuildSaveRequest(sr::ActorId{1}, network, draft));
    construction_system::Tick(MakeContextWithKnowledge(world, intents, content, knowledge));

    CHECK(knowledge.Get(network)->savedTemplates.empty());
    CHECK(content.FindShip(sr::BlueprintId("broken_ship")) == nullptr);
}

TEST_CASE("Validation errors name the failed rule rather than a generic message",
          "[construction]") {
    const ContentLibrary content = Content();
    const ShipBlueprint empty = customize_menu::NewDraft();

    const auto result = Validate(empty, content);
    REQUIRE_FALSE(result.ok());
    for (const auto& error : result.errors) {
        CHECK_FALSE(sr::ToString(error.rule).empty());
    }
}
