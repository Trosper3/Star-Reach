#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/DistressSystem.h"
#include "shared/components/Distress.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

using sr::AcknowledgeDistressRequest;
using sr::Destroyed;
using sr::DistressCall;
using sr::DistressCallRequest;
using sr::DistressType;
using sr::SensorRange;
using sr::Vec2;
using sr::Wallet;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace distress_system = sr::space::distress_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt = 1.0f / 60.0f) {
    return SystemContext{world, intents, content, dt, 0};
}

entt::entity MakeResponder(entt::registry& registry, const Vec2& position, float sensorRange) {
    const entt::entity self = registry.create();
    registry.emplace<WorldTransform>(self, position, 0.0f);
    registry.emplace<SensorRange>(self, sensorRange);
    return self;
}

DistressCall StrandedCall(const Vec2& position, float windowSeconds, int rewardCredits) {
    DistressCall call;
    call.type = DistressType::Stranded;
    call.position = position;
    call.windowSeconds = windowSeconds;
    call.rewardCredits = rewardCredits;
    return call;
}

DistressCall ShipUnderAttackCall(const Vec2& position, entt::entity npcTarget, float windowSeconds,
                                 int rewardCredits) {
    DistressCall call;
    call.type = DistressType::ShipUnderAttack;
    call.position = position;
    call.npcTarget = npcTarget;
    call.windowSeconds = windowSeconds;
    call.rewardCredits = rewardCredits;
    return call;
}

bool HasActiveCall(entt::registry& registry) {
    return registry.view<DistressCall>().begin() != registry.view<DistressCall>().end();
}

}  // namespace

TEST_CASE("DistressSystem raises a call from a request when none is active", "[distress]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity issuer = registry.create();
    registry.emplace<DistressCallRequest>(issuer, StrandedCall(Vec2{0.0f, 0.0f}, 60.0f, 500));

    distress_system::Tick(MakeContext(world, intents, content));

    CHECK(HasActiveCall(registry));
    CHECK_FALSE(registry.all_of<DistressCallRequest>(issuer));
}

TEST_CASE("DistressSystem drops a request when a call is already active", "[distress]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity first = registry.create();
    registry.emplace<DistressCallRequest>(first, StrandedCall(Vec2{0.0f, 0.0f}, 60.0f, 500));
    distress_system::Tick(MakeContext(world, intents, content));

    const entt::entity second = registry.create();
    registry.emplace<DistressCallRequest>(second, StrandedCall(Vec2{999.0f, 0.0f}, 10.0f, 1));
    distress_system::Tick(MakeContext(world, intents, content));

    int activeCount = 0;
    for (auto [entity, call] : registry.view<DistressCall>().each()) {
        (void)entity;
        ++activeCount;
        CHECK(call.rewardCredits == 500);  // Still the FIRST call, untouched.
    }
    CHECK(activeCount == 1);
}

TEST_CASE("DistressSystem acknowledges a call when a responder is within sensor range",
          "[distress]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity issuer = registry.create();
    registry.emplace<DistressCallRequest>(issuer, StrandedCall(Vec2{0.0f, 0.0f}, 60.0f, 500));

    const entt::entity responder = MakeResponder(registry, Vec2{50.0f, 0.0f}, 2000.0f);
    registry.emplace<AcknowledgeDistressRequest>(responder);

    distress_system::Tick(MakeContext(world, intents, content));

    for (auto [entity, call] : registry.view<DistressCall>().each()) {
        (void)entity;
        CHECK(call.acknowledged);
        CHECK(call.responder == responder);
    }
    CHECK_FALSE(registry.all_of<AcknowledgeDistressRequest>(responder));
}

TEST_CASE("DistressSystem does not acknowledge a call from outside sensor range", "[distress]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity issuer = registry.create();
    registry.emplace<DistressCallRequest>(issuer, StrandedCall(Vec2{0.0f, 0.0f}, 60.0f, 500));

    const entt::entity responder = MakeResponder(registry, Vec2{9000.0f, 0.0f}, 500.0f);
    registry.emplace<AcknowledgeDistressRequest>(responder);

    distress_system::Tick(MakeContext(world, intents, content));

    for (auto [entity, call] : registry.view<DistressCall>().each()) {
        (void)entity;
        CHECK_FALSE(call.acknowledged);
    }
}

TEST_CASE(
    "DistressSystem pays a Stranded call's responder once the window elapses "
    "acknowledged",
    "[distress]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity issuer = registry.create();
    registry.emplace<DistressCallRequest>(issuer, StrandedCall(Vec2{0.0f, 0.0f}, 1.0f, 500));

    const entt::entity responder = MakeResponder(registry, Vec2{0.0f, 0.0f}, 2000.0f);
    registry.emplace<AcknowledgeDistressRequest>(responder);

    distress_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK_FALSE(HasActiveCall(registry));
    REQUIRE(registry.all_of<Wallet>(responder));
    CHECK(registry.get<Wallet>(responder).credits == 500);
}

TEST_CASE("DistressSystem lets an unacknowledged Stranded call expire unpaid", "[distress]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity issuer = registry.create();
    registry.emplace<DistressCallRequest>(issuer, StrandedCall(Vec2{0.0f, 0.0f}, 1.0f, 500));
    distress_system::Tick(MakeContext(world, intents, content, 0.0f));

    distress_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK_FALSE(HasActiveCall(registry));
}

TEST_CASE("DistressSystem fails a ShipUnderAttack call the instant its target is destroyed",
          "[distress]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity npc = registry.create();

    const entt::entity issuer = registry.create();
    registry.emplace<DistressCallRequest>(issuer,
                                          ShipUnderAttackCall(Vec2{0.0f, 0.0f}, npc, 60.0f, 500));
    distress_system::Tick(MakeContext(world, intents, content, 0.0f));
    REQUIRE(HasActiveCall(registry));

    registry.emplace<Destroyed>(npc);
    distress_system::Tick(MakeContext(world, intents, content, 0.1f));

    CHECK_FALSE(HasActiveCall(registry));
}

TEST_CASE(
    "DistressSystem pays out a ShipUnderAttack call whose target survives the window, if "
    "acknowledged",
    "[distress]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity npc = registry.create();  // Never destroyed.

    const entt::entity issuer = registry.create();
    registry.emplace<DistressCallRequest>(issuer,
                                          ShipUnderAttackCall(Vec2{0.0f, 0.0f}, npc, 1.0f, 750));

    const entt::entity responder = MakeResponder(registry, Vec2{0.0f, 0.0f}, 2000.0f);
    registry.emplace<AcknowledgeDistressRequest>(responder);

    distress_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK_FALSE(HasActiveCall(registry));
    REQUIRE(registry.all_of<Wallet>(responder));
    CHECK(registry.get<Wallet>(responder).credits == 750);
}

TEST_CASE("DistressSystem resolves an unacknowledged ShipUnderAttack survival with no payout",
          "[distress]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity npc = registry.create();  // Never destroyed, never acknowledged.

    const entt::entity issuer = registry.create();
    registry.emplace<DistressCallRequest>(issuer,
                                          ShipUnderAttackCall(Vec2{0.0f, 0.0f}, npc, 1.0f, 750));

    distress_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK_FALSE(HasActiveCall(registry));
}
