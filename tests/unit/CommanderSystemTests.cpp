#include <catch2/catch_test_macros.hpp>

#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/systems/CommanderSystem.h"
#include "shared/components/Commander.h"
#include "shared/components/Health.h"
#include "shared/components/Rig.h"

using sr::Commander;
using sr::CommanderOrders;
using sr::FactionId;
using sr::Health;
using sr::KnowledgeNetworkId;
using sr::Rig;
using sr::core::knowledge::KnowledgeStore;
using sr::core::knowledge::NetworkOwnerKind;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace commander_system = sr::space::commander_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

entt::entity MakeCommanderRig(entt::registry& registry, CommanderOrders orders, float currentHull,
                              float maxHull) {
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, currentHull, maxHull);

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root, std::vector<entt::entity>{hardpoint});
    registry.emplace<Commander>(root, KnowledgeNetworkId("net"), orders, FactionId("aegis"));
    return root;
}

}  // namespace

TEST_CASE("CommanderSystem escalates a badly damaged commander to Retreat", "[commander-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = MakeCommanderRig(registry, CommanderOrders::Defend, 10.0f, 100.0f);

    commander_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Commander>(root).orders == CommanderOrders::Retreat);
}

TEST_CASE("CommanderSystem leaves a healthy commander's orders unchanged", "[commander-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = MakeCommanderRig(registry, CommanderOrders::Dispatch, 90.0f, 100.0f);

    commander_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Commander>(root).orders == CommanderOrders::Dispatch);
}

TEST_CASE("CommanderSystem does not crash on a commander with no Rig", "[commander-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<Commander>(root, KnowledgeNetworkId("net"), CommanderOrders::Defend,
                                FactionId("aegis"));

    commander_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Commander>(root).orders == CommanderOrders::Defend);
}

TEST_CASE("CommanderSystem::TickCoarse resolves standing orders the same way Tick does",
          "[commander-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = MakeCommanderRig(registry, CommanderOrders::Defend, 10.0f, 100.0f);

    commander_system::TickCoarse(MakeContext(world, intents, content));

    CHECK(registry.get<Commander>(root).orders == CommanderOrders::Retreat);
}

TEST_CASE("A commander's death releases its network reference without destroying the network",
          "[commander-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    KnowledgeStore knowledge;
    const KnowledgeNetworkId network = knowledge.Create(NetworkOwnerKind::Commander);

    const entt::entity root = registry.create();
    registry.emplace<Commander>(root, network, CommanderOrders::Defend, FactionId("aegis"));

    registry.destroy(root);

    CHECK_FALSE(registry.valid(root));
    REQUIRE(knowledge.Get(network) != nullptr);
}
