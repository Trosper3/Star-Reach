#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/HazardSystem.h"
#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"

using Catch::Approx;
using sr::Corona;
using sr::DamageType;
using sr::Destroyed;
using sr::Health;
using sr::PendingDamage;
using sr::Vec2;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace hazard_system = sr::space::hazard_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt = 1.0f) {
    return SystemContext{world, intents, content, dt, 0};
}

entt::entity MakeSun(entt::registry& registry, float range, float damagePerSecond) {
    const entt::entity sun = registry.create();
    registry.emplace<WorldTransform>(sun, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Corona>(sun, range, damagePerSecond);
    return sun;
}

entt::entity MakeHardpoint(entt::registry& registry, const Vec2& position) {
    const entt::entity entity = registry.create();
    registry.emplace<WorldTransform>(entity, position, 0.0f);
    registry.emplace<Health>(entity, 100.0f, 100.0f);
    return entity;
}

}  // namespace

TEST_CASE("HazardSystem burns a body inside corona range, scaled by depth", "[hazard]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    MakeSun(registry, 100.0f, 60.0f);
    const entt::entity hardpoint = MakeHardpoint(registry, Vec2{50.0f, 0.0f});

    hazard_system::Tick(MakeContext(world, intents, content, 1.0f));

    // distance 50 of range 100 -> falloff 0.5, damage = 60 * 0.5^2 * dt(1) = 15.
    REQUIRE(registry.all_of<PendingDamage>(hardpoint));
    const auto& pending = registry.get<PendingDamage>(hardpoint);
    CHECK(pending.amount == Approx(15.0f));
    CHECK(pending.type == DamageType::Energy);
}

TEST_CASE("HazardSystem leaves a body at or beyond corona range untouched", "[hazard]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    MakeSun(registry, 100.0f, 60.0f);
    const entt::entity hardpoint = MakeHardpoint(registry, Vec2{100.0f, 0.0f});

    hazard_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK_FALSE(registry.all_of<PendingDamage>(hardpoint));
}

TEST_CASE("HazardSystem's PendingDamage carries a null source", "[hazard]") {
    // The regression test for the AlertParty hazard (architecture.md 12.28): a real source would
    // make PartySystem::FindAttacker set the star as the party's combat target, and every escort
    // would turn and attack the sun.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    MakeSun(registry, 100.0f, 60.0f);
    const entt::entity hardpoint = MakeHardpoint(registry, Vec2{10.0f, 0.0f});

    hazard_system::Tick(MakeContext(world, intents, content, 1.0f));

    REQUIRE(registry.all_of<PendingDamage>(hardpoint));
    CHECK((registry.get<PendingDamage>(hardpoint).source == entt::null));
}

TEST_CASE("HazardSystem does not burn an already-destroyed body", "[hazard]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    MakeSun(registry, 100.0f, 60.0f);
    const entt::entity hardpoint = MakeHardpoint(registry, Vec2{10.0f, 0.0f});
    registry.emplace<Destroyed>(hardpoint);

    hazard_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK_FALSE(registry.all_of<PendingDamage>(hardpoint));
}

TEST_CASE("HazardSystem accumulates onto existing PendingDamage rather than overwriting it",
          "[hazard]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity attacker = registry.create();
    MakeSun(registry, 100.0f, 60.0f);
    const entt::entity hardpoint = MakeHardpoint(registry, Vec2{50.0f, 0.0f});
    registry.emplace<PendingDamage>(hardpoint, 5.0f, DamageType::Kinetic, attacker);

    hazard_system::Tick(MakeContext(world, intents, content, 1.0f));

    // Pre-existing 5 kinetic damage from `attacker`, plus 15 from the corona -- but the hazard's
    // QueueDamage overwrites type/source, so a real attacker's own later QueueDamage call (which
    // runs after HazardSystem in the schedule) is what actually wins the tick, not this one.
    const auto& pending = registry.get<PendingDamage>(hardpoint);
    CHECK(pending.amount == Approx(20.0f));
}
