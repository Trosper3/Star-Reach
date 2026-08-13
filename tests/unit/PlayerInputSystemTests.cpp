#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/Intent.h"
#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/DockingSystem.h"
#include "modes/space/systems/PlayerInputSystem.h"
#include "modes/space/systems/WeaponSystem.h"
#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"

using Catch::Approx;
using sr::ActorId;
using sr::ActorRef;
using sr::Docked;
using sr::FireIntent;
using sr::Rig;
using sr::ThrustInput;
using sr::Velocity;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace player_input_system = sr::space::player_input_system;
namespace docking_system = sr::space::docking_system;
namespace weapon_system = sr::space::weapon_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt = 1.0f / 60.0f) {
    return SystemContext{world, intents, content, dt, 0};
}

entt::entity MakeActor(entt::registry& registry, ActorId id) {
    const entt::entity entity = registry.create();
    registry.emplace<ActorRef>(entity, ActorRef{id});
    registry.emplace<ThrustInput>(entity);
    return entity;
}

}  // namespace

TEST_CASE("PlayerInputSystem writes ThrustInput from a held throttle intent, every fixed step",
          "[player_input]") {
    // A held key produces motion at any frame rate: FlightControls pushes one intent per real
    // frame, but the fixed-step loop may run it through Tick() more than once that frame (a slow
    // real frame covering several fixed ticks) before SpaceFlight::Update clears the queue.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity actor = MakeActor(registry, ActorId{1});
    intents.Push(sr::core::SetThrottleIntent{ActorId{1}, 1.0f, -0.5f, 0.25f});

    player_input_system::Tick(MakeContext(world, intents, content, 1.0f / 30.0f));
    CHECK(registry.get<ThrustInput>(actor).forward == Approx(1.0f));
    CHECK(registry.get<ThrustInput>(actor).strafe == Approx(-0.5f));
    CHECK(registry.get<ThrustInput>(actor).turn == Approx(0.25f));

    // Same still-pending intent, a second fixed step at a different dt: the write does not decay
    // or depend on step size.
    player_input_system::Tick(MakeContext(world, intents, content, 1.0f / 144.0f));
    CHECK(registry.get<ThrustInput>(actor).forward == Approx(1.0f));
    CHECK(registry.get<ThrustInput>(actor).strafe == Approx(-0.5f));
    CHECK(registry.get<ThrustInput>(actor).turn == Approx(0.25f));
}

TEST_CASE("PlayerInputSystem drops an intent naming an unresolvable ActorId", "[player_input]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity actor = MakeActor(registry, ActorId{1});
    // Names an actor with no ActorRef in the registry -- e.g. one that died two ticks ago on the
    // producing machine (IntentQueue.h's own contract).
    intents.Push(sr::core::SetThrottleIntent{ActorId{99}, 1.0f, 1.0f, 1.0f});

    player_input_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(actor).forward == Approx(0.0f));
    CHECK(registry.get<ThrustInput>(actor).strafe == Approx(0.0f));
    CHECK(registry.get<ThrustInput>(actor).turn == Approx(0.0f));
}

TEST_CASE("A docked player cannot thrust: DockingSystem zeroes what PlayerInputSystem wrote "
          "this same tick",
          "[player_input]") {
    // Regression for the schedule constraint that would silently break: PlayerInputSystem runs
    // before DockingSystem, so a still-Docked rig's ThrustInput ends the tick at zero regardless
    // of a held throttle intent.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity ship = MakeActor(registry, ActorId{1});
    registry.emplace<Docked>(ship, entt::null, entt::null);
    registry.emplace<Rig>(ship);
    registry.emplace<Velocity>(ship);
    intents.Push(sr::core::SetThrottleIntent{ActorId{1}, 1.0f, 0.0f, 0.0f});

    const SystemContext ctx = MakeContext(world, intents, content);
    player_input_system::Tick(ctx);
    REQUIRE(registry.get<ThrustInput>(ship).forward == Approx(1.0f));

    docking_system::Tick(ctx);
    CHECK(registry.get<ThrustInput>(ship).forward == Approx(0.0f));
}

TEST_CASE("FireIntent from a FireWeaponsIntent reaches WeaponSystem in the same tick it was "
          "pushed",
          "[player_input]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity actor = MakeActor(registry, ActorId{1});
    intents.Push(sr::core::FireWeaponsIntent{ActorId{1}});

    const SystemContext ctx = MakeContext(world, intents, content);
    player_input_system::Tick(ctx);
    REQUIRE(registry.all_of<FireIntent>(actor));

    // WeaponSystem drains and clears FireIntent unconditionally, every tick -- still visible
    // there this same tick, not delayed a tick behind the intent that produced it.
    weapon_system::Tick(ctx);
    CHECK_FALSE(registry.all_of<FireIntent>(actor));
}
