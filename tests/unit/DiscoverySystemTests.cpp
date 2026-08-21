#include <catch2/catch_test_macros.hpp>

#include <utility>

#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/systems/DiscoverySystem.h"
#include "modes/space/systems/PlayerRecordSystem.h"
#include "shared/components/Identity.h"

using sr::FactionId;
using sr::FactionRef;
using sr::PlayerControlled;
using sr::core::knowledge::FactionNetworkId;
using sr::core::knowledge::KnowledgeNetwork;
using sr::core::knowledge::KnowledgeStore;
using sr::core::knowledge::NetworkOwnerKind;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace discovery_system = sr::space::discovery_system;
namespace player_record_system = sr::space::player_record_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, KnowledgeStore& knowledge) {
    SystemContext ctx{world, intents, content, 1.0f / 60.0f, 0};
    ctx.knowledge = &knowledge;
    return ctx;
}

// Mirrors core/knowledge/KnowledgeSeeding.h's SeedFactionNetworks for one faction -- Grant() is a
// no-op against an id nothing registered, so every test that expects a write to land seeds the
// faction's network first, the same way main.cpp does once at real startup.
void SeedFaction(KnowledgeStore& knowledge, const FactionId& faction) {
    KnowledgeNetwork network;
    network.ownerKind = NetworkOwnerKind::Faction;
    knowledge.LoadNetwork(FactionNetworkId(faction), std::move(network));
}

}  // namespace

TEST_CASE("DiscoverySystem discovers the active system for the player's record faction",
          "[discovery-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;
    SeedFaction(knowledge, FactionId("aegis"));

    player_record_system::SetFaction(registry, FactionId("aegis"));

    discovery_system::Tick(MakeContext(world, intents, content, knowledge));

    const auto* network = knowledge.Get(FactionNetworkId(FactionId("aegis")));
    REQUIRE(network != nullptr);
    CHECK(network->discoveredSystems.contains("sol"));
}

TEST_CASE("DiscoverySystem does nothing without a player record", "[discovery-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;
    SeedFaction(knowledge, FactionId("reavers"));

    const entt::entity npc = registry.create();
    registry.emplace<FactionRef>(npc, FactionId("reavers"));

    discovery_system::Tick(MakeContext(world, intents, content, knowledge));

    CHECK_FALSE(
        knowledge.Get(FactionNetworkId(FactionId("reavers")))->discoveredSystems.contains("sol"));
}

TEST_CASE("DiscoverySystem does nothing when the context has no knowledge pointer",
          "[discovery-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    player_record_system::SetFaction(registry, FactionId("aegis"));

    const SystemContext ctx{world, intents, content, 1.0f / 60.0f, 0};  // knowledge defaults null.
    discovery_system::Tick(ctx);                                        // Must not crash.
}

TEST_CASE("DiscoverySystem's grant is a no-op for a faction with no seeded network",
          "[discovery-system]") {
    // features.md 8.3: "absence must never look like emptiness" is a UI concern (P5-04), but the
    // underlying failure mode this guards is the same one architecture.md 12.32 already named for
    // an unseeded DiplomacyMatrix -- a live ctx.knowledge pointer costs nothing if nobody
    // Create()'d (or seeded) the network a write targets. This must not crash.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;  // "unseeded" faction never registered.

    player_record_system::SetFaction(registry, FactionId("unseeded"));

    discovery_system::Tick(MakeContext(world, intents, content, knowledge));

    CHECK(knowledge.Get(FactionNetworkId(FactionId("unseeded"))) == nullptr);
}

TEST_CASE(
    "DiscoverySystem credits the player's own faction, not a foreign station's it is docked at",
    "[discovery-system]") {
    // architecture.md 12.30.3, amending 12.30.1: while docked, the derived PlayerControlled names
    // the station -- at a foreign one, that FactionRef is the host's, and discovery must not
    // invert to credit them. Simulates the docked shape directly: a PlayerControlled/FactionRef
    // pair under the host's flag, alongside a player record under the player's own.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;
    SeedFaction(knowledge, FactionId("aegis"));
    SeedFaction(knowledge, FactionId("zenith"));

    player_record_system::SetFaction(registry, FactionId("aegis"));
    const entt::entity foreignStation = registry.create();
    registry.emplace<PlayerControlled>(foreignStation);
    registry.emplace<FactionRef>(foreignStation, FactionId("zenith"));

    discovery_system::Tick(MakeContext(world, intents, content, knowledge));

    CHECK(knowledge.Get(FactionNetworkId(FactionId("aegis")))->discoveredSystems.contains("sol"));
    CHECK_FALSE(
        knowledge.Get(FactionNetworkId(FactionId("zenith")))->discoveredSystems.contains("sol"));
}
