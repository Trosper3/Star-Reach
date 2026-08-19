#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/NpcAiSystem.h"
#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Identity.h"
#include "shared/components/Orbit.h"
#include "shared/components/Physics.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"

using Catch::Approx;
using sr::Docked;
using sr::FireIntent;
using sr::GravityWell;
using sr::PlayerLocation;
using sr::Target;
using sr::ThrustInput;
using sr::Vec2;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace npc_ai_system = sr::space::npc_ai_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

// An AI-driven rig at the origin, facing +x, with no target yet. Returns its entity.
entt::entity MakeSeeker(entt::registry& registry) {
    const entt::entity seeker = registry.create();
    registry.emplace<WorldTransform>(seeker, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<ThrustInput>(seeker);
    registry.emplace<Target>(seeker);
    return seeker;
}

entt::entity MakeTargetRig(entt::registry& registry, const Vec2& position) {
    const entt::entity rig = registry.create();
    registry.emplace<WorldTransform>(rig, position, 0.0f);
    return rig;
}

}  // namespace

TEST_CASE("NpcAiSystem does nothing for a seeker with no acquired target", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.0f));
    CHECK_FALSE(registry.all_of<FireIntent>(seeker));
}

TEST_CASE("NpcAiSystem thrusts forward and requests fire once aimed at a far target", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    const entt::entity enemy = MakeTargetRig(registry, Vec2{1000.0f, 0.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(1.0f));
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.0f));
    CHECK(registry.all_of<FireIntent>(seeker));
}

TEST_CASE("NpcAiSystem turns toward a target that is not dead ahead", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    // Directly "above" the seeker (+y): a 90-degree heading error.
    const entt::entity enemy = MakeTargetRig(registry, Vec2{0.0f, 1000.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    // Half the turn command at a quarter-turn heading error (proportional, clamped at +-1).
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.5f));
    // Broadside to the target: do not burn the main engine into a worse angle.
    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
}

TEST_CASE("NpcAiSystem stops closing once within engagement range but keeps requesting fire",
          "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    const entt::entity enemy = MakeTargetRig(registry, Vec2{100.0f, 0.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
    CHECK(registry.all_of<FireIntent>(seeker));
}

TEST_CASE("NpcAiSystem clears stale thrust and fire intent once a target is lost", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    registry.get<ThrustInput>(seeker) = ThrustInput{1.0f, 0.0f, 1.0f};
    registry.emplace<FireIntent>(seeker);
    // Target.rig stays entt::null (never acquired / lost by TargetingSystem this tick).

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.0f));
    CHECK_FALSE(registry.all_of<FireIntent>(seeker));
}

TEST_CASE("NpcAiSystem never drives a Docked rig, even with a live target", "[npc_ai]") {
    // Regression test for architecture.md 13.3 finding H: a docked NPC must not have its throttle
    // rewritten or FireIntent re-emplaced -- DockingSystem zeroes both once, on dock, and an
    // unexcluded NpcAiSystem would undo that every tick after.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    registry.emplace<Docked>(seeker);
    const entt::entity enemy = MakeTargetRig(registry, Vec2{1000.0f, 0.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.0f));
    CHECK_FALSE(registry.all_of<FireIntent>(seeker));
}

TEST_CASE("NpcAiSystem flees a GravityWell instead of pursuing a target across it", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    // Well centred at the origin; seeker sits well inside range + the avoidance margin.
    const entt::entity sun = registry.create();
    registry.emplace<WorldTransform>(sun, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<GravityWell>(sun, 2200.0f, 260.0f);

    // MakeSeeker's default rotation (0, i.e. +x) already faces directly away from a well
    // centred at the origin -- no turn needed, just a straight burn clear.
    const entt::entity seeker = MakeSeeker(registry);
    registry.get<WorldTransform>(seeker).position = Vec2{500.0f, 0.0f};
    // Target on the far side of the well: straight-line pursuit would cut through it.
    const entt::entity enemy = MakeTargetRig(registry, Vec2{-500.0f, 0.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.0f));
    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(1.0f));
    CHECK_FALSE(registry.all_of<FireIntent>(seeker));
}

TEST_CASE("NpcAiSystem pursues normally once outside every GravityWell's avoidance range",
          "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity sun = registry.create();
    registry.emplace<WorldTransform>(sun, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<GravityWell>(sun, 2200.0f, 260.0f);

    const entt::entity seeker = MakeSeeker(registry);
    registry.get<WorldTransform>(seeker).position = Vec2{3000.0f, 0.0f};  // Outside range + margin.
    const entt::entity enemy = MakeTargetRig(registry, Vec2{4000.0f, 0.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(1.0f));
    CHECK(registry.all_of<FireIntent>(seeker));
}

TEST_CASE("NpcAiSystem never drives the PlayerLocation rig", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity player = MakeSeeker(registry);
    registry.emplace<PlayerLocation>(player, PlayerLocation{player});
    const entt::entity enemy = MakeTargetRig(registry, Vec2{1000.0f, 0.0f});
    registry.get<Target>(player).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(player).forward == Approx(0.0f));
    CHECK_FALSE(registry.all_of<FireIntent>(player));
}
