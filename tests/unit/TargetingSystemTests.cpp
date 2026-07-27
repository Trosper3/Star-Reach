#include <catch2/catch_test_macros.hpp>
#include <utility>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/TargetingSystem.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

using sr::Destroyed;
using sr::FactionId;
using sr::FactionRef;
using sr::Rig;
using sr::SensorRange;
using sr::Target;
using sr::Targetable;
using sr::Vec2;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace targeting_system = sr::space::targeting_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

entt::entity MakeRig(entt::registry& registry, const Vec2& position, const FactionId& faction) {
    const entt::entity rig = registry.create();
    registry.emplace<WorldTransform>(rig, position, 0.0f);
    registry.emplace<FactionRef>(rig, faction);
    registry.emplace<Targetable>(rig);
    return rig;
}

entt::entity MakeHardpoint(entt::registry& registry, entt::entity rig, const Vec2& position) {
    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, position, 0.0f);
    registry.get<Rig>(rig).children.push_back(hardpoint);
    return hardpoint;
}

}  // namespace

TEST_CASE("TargetingSystem acquires the nearest hostile Targetable rig in range", "[targeting]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = registry.create();
    registry.emplace<WorldTransform>(seeker, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<FactionRef>(seeker, FactionId{"blue"});
    registry.emplace<SensorRange>(seeker, 100.0f);
    registry.emplace<Target>(seeker);

    const entt::entity farEnemy = MakeRig(registry, Vec2{50.0f, 0.0f}, FactionId{"red"});
    const entt::entity nearEnemy = MakeRig(registry, Vec2{10.0f, 0.0f}, FactionId{"red"});
    MakeRig(registry, Vec2{5.0f, 0.0f},
            FactionId{"blue"});  // Friendly and closer; must be skipped.

    targeting_system::Tick(MakeContext(world, intents, content));

    const auto& target = registry.get<Target>(seeker);
    CHECK(target.rig == nearEnemy);
    CHECK(target.rig != farEnemy);
}

TEST_CASE("TargetingSystem does not acquire a hostile rig outside sensor range", "[targeting]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = registry.create();
    registry.emplace<WorldTransform>(seeker, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<FactionRef>(seeker, FactionId{"blue"});
    registry.emplace<SensorRange>(seeker, 10.0f);
    registry.emplace<Target>(seeker);

    MakeRig(registry, Vec2{100.0f, 0.0f}, FactionId{"red"});

    targeting_system::Tick(MakeContext(world, intents, content));

    CHECK((registry.get<Target>(seeker).rig == entt::null));
}

TEST_CASE("TargetingSystem selects the nearest living hardpoint of the acquired rig",
          "[targeting]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = registry.create();
    registry.emplace<WorldTransform>(seeker, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<FactionRef>(seeker, FactionId{"blue"});
    registry.emplace<SensorRange>(seeker, 100.0f);
    registry.emplace<Target>(seeker);

    const entt::entity enemy = MakeRig(registry, Vec2{20.0f, 0.0f}, FactionId{"red"});
    registry.emplace<Rig>(enemy);
    const entt::entity farHardpoint = MakeHardpoint(registry, enemy, Vec2{25.0f, 0.0f});
    const entt::entity nearHardpoint = MakeHardpoint(registry, enemy, Vec2{20.0f, 0.0f});
    (void)farHardpoint;

    targeting_system::Tick(MakeContext(world, intents, content));

    const auto& target = registry.get<Target>(seeker);
    CHECK(target.rig == enemy);
    CHECK(target.hardpoint == nearHardpoint);
}

TEST_CASE("TargetingSystem re-selects the aim point once the current hardpoint is destroyed",
          "[targeting]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = registry.create();
    registry.emplace<WorldTransform>(seeker, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<FactionRef>(seeker, FactionId{"blue"});
    registry.emplace<SensorRange>(seeker, 100.0f);

    const entt::entity enemy = MakeRig(registry, Vec2{20.0f, 0.0f}, FactionId{"red"});
    registry.emplace<Rig>(enemy);
    const entt::entity nearHardpoint = MakeHardpoint(registry, enemy, Vec2{20.0f, 0.0f});
    const entt::entity farHardpoint = MakeHardpoint(registry, enemy, Vec2{25.0f, 0.0f});

    Target target;
    target.rig = enemy;
    target.hardpoint = nearHardpoint;
    registry.emplace<Target>(seeker, target);
    registry.emplace<Destroyed>(nearHardpoint);

    targeting_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Target>(seeker).hardpoint == farHardpoint);
}

TEST_CASE("TargetingSystem drops a target that is no longer valid and re-acquires if possible",
          "[targeting]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = registry.create();
    registry.emplace<WorldTransform>(seeker, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<FactionRef>(seeker, FactionId{"blue"});
    registry.emplace<SensorRange>(seeker, 100.0f);

    const entt::entity stale = registry.create();  // No Targetable/FactionRef -- an invalid target.
    Target target;
    target.rig = stale;
    registry.emplace<Target>(seeker, target);

    const entt::entity replacement = MakeRig(registry, Vec2{10.0f, 0.0f}, FactionId{"red"});

    targeting_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Target>(seeker).rig == replacement);
}

TEST_CASE("TargetingSystem leaves a seeker with no hostile candidates untargeted", "[targeting]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = registry.create();
    registry.emplace<WorldTransform>(seeker, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<FactionRef>(seeker, FactionId{"blue"});
    registry.emplace<SensorRange>(seeker, 100.0f);
    registry.emplace<Target>(seeker);

    MakeRig(registry, Vec2{10.0f, 0.0f}, FactionId{"blue"});  // Friendly only.

    CHECK_NOTHROW(targeting_system::Tick(MakeContext(world, intents, content)));
    CHECK((registry.get<Target>(seeker).rig == entt::null));
}
