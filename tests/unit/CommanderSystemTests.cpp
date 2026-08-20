#include <catch2/catch_test_macros.hpp>

#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/systems/CommanderSystem.h"
#include "shared/components/Commander.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

using sr::Commander;
using sr::CommanderOrders;
using sr::Destroyed;
using sr::FactionId;
using sr::FactionRef;
using sr::Health;
using sr::KnowledgeNetworkId;
using sr::ParentRig;
using sr::Rig;
using sr::Target;
using sr::Targetable;
using sr::Vec2;
using sr::WorldTransform;
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

// A commanded vessel: a root Rig with a structural hardpoint (Health) and a separate Bridge
// hardpoint carrying Commander, linked back to the root via ParentRig -- architecture.md 12.16
// item 22's "Commander lives on the hardpoint, not the root."
entt::entity MakeCommanderRig(entt::registry& registry, CommanderOrders orders, float currentHull,
                              float maxHull, entt::entity& outBridge) {
    const entt::entity hull = registry.create();
    registry.emplace<Health>(hull, currentHull, maxHull);

    const entt::entity root = registry.create();
    const entt::entity bridge = registry.create();
    registry.emplace<ParentRig>(bridge, root);
    registry.emplace<Commander>(bridge, KnowledgeNetworkId("net"), orders, FactionId("aegis"));
    registry.emplace<Rig>(root, std::vector<entt::entity>{hull, bridge});
    registry.emplace<WorldTransform>(root, Vec2{0.0f, 0.0f}, 0.0f);

    outBridge = bridge;
    return root;
}

}  // namespace

TEST_CASE("CommanderSystem escalates a badly damaged commander to Retreat", "[commander-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    entt::entity bridge = entt::null;
    MakeCommanderRig(registry, CommanderOrders::Defend, 10.0f, 100.0f, bridge);

    commander_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Commander>(bridge).orders == CommanderOrders::Retreat);
}

TEST_CASE("CommanderSystem leaves a healthy commander's orders unchanged", "[commander-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    entt::entity bridge = entt::null;
    MakeCommanderRig(registry, CommanderOrders::Dispatch, 90.0f, 100.0f, bridge);

    commander_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Commander>(bridge).orders == CommanderOrders::Dispatch);
}

TEST_CASE("CommanderSystem does not crash on a commander whose vessel has no Rig",
          "[commander-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rootWithNoRig = registry.create();
    const entt::entity bridge = registry.create();
    registry.emplace<ParentRig>(bridge, rootWithNoRig);
    registry.emplace<Commander>(bridge, KnowledgeNetworkId("net"), CommanderOrders::Defend,
                                FactionId("aegis"));

    commander_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Commander>(bridge).orders == CommanderOrders::Defend);
}

TEST_CASE("A commander's death releases its network reference without destroying the network",
          "[commander-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    KnowledgeStore knowledge;
    const KnowledgeNetworkId network = knowledge.Create(NetworkOwnerKind::Commander);

    const entt::entity bridge = registry.create();
    registry.emplace<Commander>(bridge, network, CommanderOrders::Defend, FactionId("aegis"));

    registry.destroy(bridge);

    CHECK_FALSE(registry.valid(bridge));
    REQUIRE(knowledge.Get(network) != nullptr);
}

TEST_CASE(
    "Destroying a commander's bridge removes its leadership without touching the rest of the "
    "vessel",
    "[commander-system]") {
    // architecture.md 12.16 item 22: "destroy the bridge, lose the commander, keep the ship."
    // DamageSystem only tags Destroyed, never strips components (Rig.h), so this checks
    // HasLeadership rather than the Commander component's presence.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    entt::entity bridge = entt::null;
    const entt::entity root =
        MakeCommanderRig(registry, CommanderOrders::Defend, 100.0f, 100.0f, bridge);
    const entt::entity hull = registry.get<Rig>(root).children[0];

    registry.emplace<Destroyed>(bridge);

    CHECK_FALSE(commander_system::HasLeadership(registry, FactionId("aegis")));
    CHECK_FALSE(registry.all_of<Destroyed>(root));
    CHECK_FALSE(registry.all_of<Destroyed>(hull));
    REQUIRE(registry.all_of<Rig>(root));
    CHECK(registry.get<Rig>(root).children.size() == 2);
}

TEST_CASE("HasLeadership is true while a faction has at least one living commander",
          "[commander-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    entt::entity bridge = entt::null;
    MakeCommanderRig(registry, CommanderOrders::Defend, 100.0f, 100.0f, bridge);

    CHECK(commander_system::HasLeadership(registry, FactionId("aegis")));
}

TEST_CASE(
    "HasLeadership reports false for a faction with no living commander, the player's faction "
    "evaluated by the identical predicate",
    "[commander-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    CHECK_FALSE(commander_system::HasLeadership(registry, FactionId("reavers")));

    // The player's faction has no special path: an empty registry reports no leadership for it
    // exactly as it would for any AI faction, and a living commander under any FactionId
    // (including "player") satisfies it exactly as "aegis" does above.
    const entt::entity root = registry.create();
    const entt::entity playerBridge = registry.create();
    registry.emplace<ParentRig>(playerBridge, root);
    registry.emplace<Commander>(playerBridge, KnowledgeNetworkId("player-net"),
                                CommanderOrders::Defend, FactionId("player"));
    registry.emplace<Rig>(root, std::vector<entt::entity>{playerBridge});

    CHECK(commander_system::HasLeadership(registry, FactionId("player")));
    CHECK_FALSE(commander_system::HasLeadership(registry, FactionId("reavers")));
}

TEST_CASE(
    "A commander dispatches the nearest idle same-faction rig to a threat near its own vessel",
    "[commander-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    entt::entity bridge = entt::null;
    MakeCommanderRig(registry, CommanderOrders::Defend, 100.0f, 100.0f, bridge);

    const entt::entity threat = registry.create();
    registry.emplace<WorldTransform>(threat, Vec2{1000.0f, 0.0f}, 0.0f);
    registry.emplace<FactionRef>(threat, FactionId("reavers"));
    registry.emplace<Targetable>(threat);

    const entt::entity defender = registry.create();
    registry.emplace<WorldTransform>(defender, Vec2{1100.0f, 0.0f}, 0.0f);
    registry.emplace<FactionRef>(defender, FactionId("aegis"));
    registry.emplace<Targetable>(defender);
    registry.emplace<Target>(defender);

    commander_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Target>(defender).rig == threat);
}

TEST_CASE("A Retreating commander does not dispatch a defender", "[commander-system]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    entt::entity bridge = entt::null;
    MakeCommanderRig(registry, CommanderOrders::Retreat, 100.0f, 100.0f, bridge);

    const entt::entity threat = registry.create();
    registry.emplace<WorldTransform>(threat, Vec2{1000.0f, 0.0f}, 0.0f);
    registry.emplace<FactionRef>(threat, FactionId("reavers"));
    registry.emplace<Targetable>(threat);

    const entt::entity defender = registry.create();
    registry.emplace<WorldTransform>(defender, Vec2{1100.0f, 0.0f}, 0.0f);
    registry.emplace<FactionRef>(defender, FactionId("aegis"));
    registry.emplace<Targetable>(defender);
    registry.emplace<Target>(defender);

    commander_system::Tick(MakeContext(world, intents, content));

    CHECK((registry.get<Target>(defender).rig == entt::null));
}
