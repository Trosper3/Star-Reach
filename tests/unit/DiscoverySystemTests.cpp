#include <catch2/catch_test_macros.hpp>

#include "core/galaxy/Discovery.h"
#include "modes/space/systems/DiscoverySystem.h"
#include "shared/components/Identity.h"

using sr::FactionId;
using sr::FactionRef;
using sr::PlayerControlled;
using sr::core::galaxy::DiscoveryState;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace discovery_system = sr::space::discovery_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, DiscoveryState& discovery) {
    SystemContext ctx{world, intents, content, 1.0f / 60.0f, 0};
    ctx.discovery = &discovery;
    return ctx;
}

}  // namespace

TEST_CASE("DiscoverySystem discovers the active system for the player's faction",
          "[discovery-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiscoveryState discovery;

    const entt::entity player = registry.create();
    registry.emplace<PlayerControlled>(player);
    registry.emplace<FactionRef>(player, FactionId("aegis"));

    discovery_system::Tick(MakeContext(world, intents, content, discovery));

    CHECK(discovery.IsDiscovered(FactionId("aegis"), "sol"));
}

TEST_CASE("DiscoverySystem does nothing without a PlayerControlled entity", "[discovery-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiscoveryState discovery;

    const entt::entity npc = registry.create();
    registry.emplace<FactionRef>(npc, FactionId("reavers"));

    discovery_system::Tick(MakeContext(world, intents, content, discovery));

    CHECK_FALSE(discovery.IsDiscovered(FactionId("reavers"), "sol"));
}

TEST_CASE("DiscoverySystem does nothing when the context has no discovery pointer",
          "[discovery-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity player = registry.create();
    registry.emplace<PlayerControlled>(player);
    registry.emplace<FactionRef>(player, FactionId("aegis"));

    const SystemContext ctx{world, intents, content, 1.0f / 60.0f, 0};  // discovery defaults null.
    discovery_system::Tick(ctx);                                        // Must not crash.
}

TEST_CASE("DiscoverySystem::TickCoarse behaves the same as Tick", "[discovery-system]") {
    SystemWorld world("kepler");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiscoveryState discovery;

    const entt::entity player = registry.create();
    registry.emplace<PlayerControlled>(player);
    registry.emplace<FactionRef>(player, FactionId("aegis"));

    discovery_system::TickCoarse(MakeContext(world, intents, content, discovery));

    CHECK(discovery.IsDiscovered(FactionId("aegis"), "kepler"));
}
