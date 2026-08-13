#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/TutorialSystem.h"
#include "shared/components/Docking.h"
#include "shared/components/Loot.h"
#include "shared/components/Mining.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/components/Tutorial.h"
#include "shared/rig/CargoView.h"

using sr::Asteroid;
using sr::CargoHold;
using sr::Destroyed;
using sr::Docked;
using sr::ItemKind;
using sr::ItemStack;
using sr::ParentRig;
using sr::Rig;
using sr::Target;
using sr::Tutorial;
using sr::TutorialStep;
using sr::Vec2;
using sr::WorldTransform;
namespace cargo_view = sr::cargo_view;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace tutorial_system = sr::space::tutorial_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

entt::entity MakeRig(entt::registry& registry, const Vec2& position, TutorialStep step) {
    const entt::entity self = registry.create();
    registry.emplace<WorldTransform>(self, position, 0.0f);
    Tutorial tutorial;
    tutorial.step = step;
    registry.emplace<Tutorial>(self, tutorial);
    return self;
}

}  // namespace

TEST_CASE("TutorialSystem captures the starting position on the first tick", "[tutorial]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{100.0f, 50.0f}, TutorialStep::Move);

    tutorial_system::Tick(MakeContext(world, intents, content));

    const Tutorial& tutorial = registry.get<Tutorial>(rig);
    CHECK(tutorial.started);
    CHECK(tutorial.startPosition.x == 100.0f);
    CHECK(tutorial.startPosition.y == 50.0f);
}

TEST_CASE("TutorialSystem advances Move once the rig travels far enough", "[tutorial]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Move);
    tutorial_system::Tick(MakeContext(world, intents, content));  // Captures startPosition.

    registry.get<WorldTransform>(rig).position = Vec2{400.0f, 0.0f};
    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::Target);
}

TEST_CASE("TutorialSystem does not advance Move before the distance threshold", "[tutorial]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Move);
    tutorial_system::Tick(MakeContext(world, intents, content));

    registry.get<WorldTransform>(rig).position = Vec2{100.0f, 0.0f};
    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::Move);
}

TEST_CASE("TutorialSystem advances Target once the rig has a live Target", "[tutorial]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Target);
    const entt::entity someRig = registry.create();
    registry.emplace<Target>(rig, someRig, entt::null);

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::DestroyAsteroid);
}

TEST_CASE("TutorialSystem advances DestroyAsteroid when any asteroid is destroyed this tick",
          "[tutorial]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::DestroyAsteroid);

    const entt::entity asteroid = registry.create();
    registry.emplace<Asteroid>(asteroid);
    registry.emplace<Destroyed>(asteroid);

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::CollectElement);
}

TEST_CASE("TutorialSystem does not advance DestroyAsteroid without a destroyed asteroid",
          "[tutorial]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::DestroyAsteroid);

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::DestroyAsteroid);
}

TEST_CASE("TutorialSystem advances CollectElement once the rig's CargoHold has an element",
          "[tutorial]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::CollectElement);
    const entt::entity bay = registry.create();
    registry.emplace<ParentRig>(bay, rig);
    registry.emplace<CargoHold>(bay, std::vector<ItemStack>{}, 10, 1000.0f);
    registry.emplace<Rig>(rig, std::vector<entt::entity>{bay});
    cargo_view::Deposit(registry, rig, ItemStack{ItemKind::Element, "Fe", 1, 2.0f});

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::Dock);
}

TEST_CASE("TutorialSystem advances Dock once the rig is tagged Docked", "[tutorial]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Dock);
    registry.emplace<Docked>(rig, entt::null, entt::null);

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::Sell);
}

TEST_CASE("TutorialSystem never auto-advances Sell, EquipModule, or Warp", "[tutorial]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity sell = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Sell);
    const entt::entity equip = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::EquipModule);
    const entt::entity warp = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Warp);

    for (int i = 0; i < 10; ++i) {
        tutorial_system::Tick(MakeContext(world, intents, content));
    }

    CHECK(registry.get<Tutorial>(sell).step == TutorialStep::Sell);
    CHECK(registry.get<Tutorial>(equip).step == TutorialStep::EquipModule);
    CHECK(registry.get<Tutorial>(warp).step == TutorialStep::Warp);
}

TEST_CASE("TutorialSystem leaves a rig with no Tutorial component untouched", "[tutorial]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = registry.create();
    registry.emplace<WorldTransform>(rig, Vec2{0.0f, 0.0f}, 0.0f);

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<Tutorial>(rig));
}
