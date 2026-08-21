#include <catch2/catch_test_macros.hpp>

#include "core/diplomacy/Reputation.h"
#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/CommsSystem.h"
#include "shared/components/Comms.h"
#include "shared/components/Identity.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

using sr::CommsLog;
using sr::CommsLogSingleton;
using sr::DisplayName;
using sr::FactionId;
using sr::FactionRef;
using sr::HailRequest;
using sr::SensorRange;
using sr::Vec2;
using sr::WorldTransform;
using sr::core::diplomacy::Reputation;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace comms_system = sr::space::comms_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 3};
}

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, Reputation& reputation) {
    return SystemContext{world,   intents, content, 1.0f / 60.0f, 3,
                         nullptr, nullptr, nullptr, nullptr,      &reputation};
}

entt::entity MakeHailer(entt::registry& registry, const Vec2& position, float sensorRange) {
    const entt::entity self = registry.create();
    registry.emplace<WorldTransform>(self, position, 0.0f);
    registry.emplace<SensorRange>(self, sensorRange);
    return self;
}

const CommsLog* FindLog(entt::registry& registry) {
    for (auto [entity, log] : registry.view<CommsLogSingleton, CommsLog>().each()) {
        (void)entity;
        return &log;
    }
    return nullptr;
}

}  // namespace

TEST_CASE("CommsSystem logs a hail and a response when the target is in sensor range", "[comms]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity target = registry.create();
    registry.emplace<WorldTransform>(target, Vec2{100.0f, 0.0f}, 0.0f);
    registry.emplace<DisplayName>(target, "Aegis Outpost");

    const entt::entity hailer = MakeHailer(registry, Vec2{0.0f, 0.0f}, 2000.0f);
    registry.emplace<HailRequest>(hailer, target);

    comms_system::Tick(MakeContext(world, intents, content));

    const CommsLog* log = FindLog(registry);
    REQUIRE(log != nullptr);
    REQUIRE(log->entries.size() == 2);
    CHECK(log->entries[0].text == "Hailing Aegis Outpost...");
    CHECK(log->entries[0].fromPlayer);
    CHECK(log->entries[1].text.rfind("Aegis Outpost: ", 0) == 0);
    CHECK_FALSE(log->entries[1].fromPlayer);
    CHECK_FALSE(registry.all_of<HailRequest>(hailer));
}

TEST_CASE("CommsSystem logs no response when the target is out of sensor range", "[comms]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity target = registry.create();
    registry.emplace<WorldTransform>(target, Vec2{9000.0f, 0.0f}, 0.0f);
    registry.emplace<DisplayName>(target, "Reaver Raider");

    const entt::entity hailer = MakeHailer(registry, Vec2{0.0f, 0.0f}, 500.0f);
    registry.emplace<HailRequest>(hailer, target);

    comms_system::Tick(MakeContext(world, intents, content));

    const CommsLog* log = FindLog(registry);
    REQUIRE(log != nullptr);
    REQUIRE(log->entries.size() == 2);
    CHECK(log->entries[1].text == "Reaver Raider does not respond -- out of range.");
}

TEST_CASE("CommsSystem falls back to 'Unknown contact' for a target with no DisplayName",
          "[comms]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity target = registry.create();
    registry.emplace<WorldTransform>(target, Vec2{10.0f, 0.0f}, 0.0f);

    const entt::entity hailer = MakeHailer(registry, Vec2{0.0f, 0.0f}, 2000.0f);
    registry.emplace<HailRequest>(hailer, target);

    comms_system::Tick(MakeContext(world, intents, content));

    const CommsLog* log = FindLog(registry);
    REQUIRE(log != nullptr);
    CHECK(log->entries[0].text == "Hailing Unknown contact...");
}

TEST_CASE("CommsSystem drops a HailRequest naming an invalid target without logging anything",
          "[comms]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    // A genuinely stale handle: created then destroyed, the same technique
    // SystemWorldTests uses for "a destroyed entity's id resolves to null".
    const entt::entity stale = registry.create();
    registry.destroy(stale);

    const entt::entity hailer = MakeHailer(registry, Vec2{0.0f, 0.0f}, 2000.0f);
    registry.emplace<HailRequest>(hailer, stale);

    comms_system::Tick(MakeContext(world, intents, content));

    CHECK(FindLog(registry) == nullptr);
    CHECK_FALSE(registry.all_of<HailRequest>(hailer));
}

TEST_CASE("CommsLog retains only the most recent kCommsLogCap (8) entries", "[comms]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity target = registry.create();
    registry.emplace<WorldTransform>(target, Vec2{10.0f, 0.0f}, 0.0f);
    registry.emplace<DisplayName>(target, "Beacon");

    const entt::entity hailer = MakeHailer(registry, Vec2{0.0f, 0.0f}, 2000.0f);

    // 5 hails x 2 entries each = 10, over the 8-entry cap.
    for (int i = 0; i < 5; ++i) {
        registry.emplace_or_replace<HailRequest>(hailer, target);
        comms_system::Tick(MakeContext(world, intents, content));
    }

    const CommsLog* log = FindLog(registry);
    REQUIRE(log != nullptr);
    CHECK(log->entries.size() == 8);
}

TEST_CASE("CommsSystem raises the target faction's reputation on a successful hail", "[comms]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    Reputation reputation;

    const entt::entity target = registry.create();
    registry.emplace<WorldTransform>(target, Vec2{100.0f, 0.0f}, 0.0f);
    registry.emplace<DisplayName>(target, "Aegis Outpost");
    registry.emplace<FactionRef>(target, FactionId("aegis"));

    const entt::entity hailer = MakeHailer(registry, Vec2{0.0f, 0.0f}, 2000.0f);
    registry.emplace<HailRequest>(hailer, target);

    comms_system::Tick(MakeContext(world, intents, content, reputation));

    CHECK(reputation.Score(FactionId("aegis")) > 0.0f);
}

TEST_CASE("CommsSystem leaves reputation untouched when the hail is out of range", "[comms]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    Reputation reputation;

    const entt::entity target = registry.create();
    registry.emplace<WorldTransform>(target, Vec2{9000.0f, 0.0f}, 0.0f);
    registry.emplace<DisplayName>(target, "Reaver Raider");
    registry.emplace<FactionRef>(target, FactionId("reavers"));

    const entt::entity hailer = MakeHailer(registry, Vec2{0.0f, 0.0f}, 500.0f);
    registry.emplace<HailRequest>(hailer, target);

    comms_system::Tick(MakeContext(world, intents, content, reputation));

    CHECK(reputation.Score(FactionId("reavers")) == 0.0f);
}

TEST_CASE("CommsSystem does not touch reputation for a target with no FactionRef", "[comms]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    Reputation reputation;

    const entt::entity target = registry.create();
    registry.emplace<WorldTransform>(target, Vec2{100.0f, 0.0f}, 0.0f);
    registry.emplace<DisplayName>(target, "Aegis Outpost");

    const entt::entity hailer = MakeHailer(registry, Vec2{0.0f, 0.0f}, 2000.0f);
    registry.emplace<HailRequest>(hailer, target);

    comms_system::Tick(MakeContext(world, intents, content, reputation));

    CHECK(reputation.Score(FactionId("aegis")) == 0.0f);
}
