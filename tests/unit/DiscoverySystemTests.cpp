#include <catch2/catch_test_macros.hpp>

#include <utility>

#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/systems/DiscoverySystem.h"
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

TEST_CASE("DiscoverySystem discovers the active system for the player's faction",
          "[discovery-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;
    SeedFaction(knowledge, FactionId("aegis"));

    const entt::entity player = registry.create();
    registry.emplace<PlayerControlled>(player);
    registry.emplace<FactionRef>(player, FactionId("aegis"));

    discovery_system::Tick(MakeContext(world, intents, content, knowledge));

    const auto* network = knowledge.Get(FactionNetworkId(FactionId("aegis")));
    REQUIRE(network != nullptr);
    CHECK(network->discoveredSystems.contains("sol"));
}

TEST_CASE("DiscoverySystem does nothing without a PlayerControlled entity", "[discovery-system]") {
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

    const entt::entity player = registry.create();
    registry.emplace<PlayerControlled>(player);
    registry.emplace<FactionRef>(player, FactionId("aegis"));

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

    const entt::entity player = registry.create();
    registry.emplace<PlayerControlled>(player);
    registry.emplace<FactionRef>(player, FactionId("unseeded"));

    discovery_system::Tick(MakeContext(world, intents, content, knowledge));

    CHECK(knowledge.Get(FactionNetworkId(FactionId("unseeded"))) == nullptr);
}

TEST_CASE("DiscoverySystem's writes are shared: two allies land on the same network",
          "[discovery-system]") {
    // features.md 8.3: "a faction's members share what any one of them has found." Coverage is
    // per NETWORK, not per entity -- two PlayerControlled rigs under the same FactionRef write
    // (and therefore read) the identical KnowledgeNetwork by construction, with no separate
    // pooling step required.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    KnowledgeStore knowledge;
    SeedFaction(knowledge, FactionId("aegis"));

    const entt::entity allyOne = registry.create();
    registry.emplace<PlayerControlled>(allyOne);
    registry.emplace<FactionRef>(allyOne, FactionId("aegis"));
    const entt::entity allyTwo = registry.create();
    registry.emplace<PlayerControlled>(allyTwo);
    registry.emplace<FactionRef>(allyTwo, FactionId("aegis"));

    discovery_system::Tick(MakeContext(world, intents, content, knowledge));

    CHECK(knowledge.Get(FactionNetworkId(FactionId("aegis")))->discoveredSystems.size() == 1);
}
