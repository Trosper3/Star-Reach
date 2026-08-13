#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/MiningSystem.h"
#include "shared/components/Loot.h"
#include "shared/components/Mining.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"

using sr::Asteroid;
using sr::AsteroidComposition;
using sr::BodyKind;
using sr::Destroyed;
using sr::ElementChance;
using sr::ElementDrop;
using sr::Vec2;
using sr::WorldBody;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace mining_system = sr::space::mining_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 5};
}

entt::entity MakeDepletedAsteroid(entt::registry& registry, const Vec2& position,
                                  std::vector<ElementChance> elements) {
    const entt::entity asteroid = registry.create();
    registry.emplace<WorldTransform>(asteroid, position, 0.0f);
    registry.emplace<Asteroid>(asteroid);
    registry.emplace<AsteroidComposition>(asteroid, std::move(elements));
    registry.emplace<Destroyed>(asteroid);
    return asteroid;
}

int CountElementDrops(entt::registry& registry) {
    int count = 0;
    for (auto [drop, element] : registry.view<ElementDrop>().each()) {
        (void)drop;
        (void)element;
        ++count;
    }
    return count;
}

}  // namespace

TEST_CASE("MiningSystem destroys a depleted asteroid", "[mining]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity asteroid = MakeDepletedAsteroid(registry, Vec2{0.0f, 0.0f}, {{"iron", 100}});

    mining_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.valid(asteroid));
}

TEST_CASE("MiningSystem always spawns an ElementDrop for a guaranteed (100%) element", "[mining]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    MakeDepletedAsteroid(registry, Vec2{120.0f, -40.0f}, {{"iron", 100}});

    mining_system::Tick(MakeContext(world, intents, content));

    REQUIRE(CountElementDrops(registry) == 1);
    for (auto [drop, element, xf] : registry.view<ElementDrop, WorldTransform>().each()) {
        CHECK(element.elementId == "iron");
        CHECK(xf.position.x == 120.0f);
        CHECK(xf.position.y == -40.0f);
        REQUIRE(registry.all_of<WorldBody>(drop));
        CHECK(registry.get<WorldBody>(drop).kind == BodyKind::Drop);
    }
}

TEST_CASE("MiningSystem never spawns an ElementDrop for an impossible (0%) element", "[mining]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    MakeDepletedAsteroid(registry, Vec2{0.0f, 0.0f}, {{"voidstone", 0}});

    mining_system::Tick(MakeContext(world, intents, content));

    CHECK(CountElementDrops(registry) == 0);
}

TEST_CASE("MiningSystem can roll multiple independent elements from one asteroid", "[mining]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    MakeDepletedAsteroid(registry, Vec2{0.0f, 0.0f}, {{"iron", 100}, {"carbon", 100}});

    mining_system::Tick(MakeContext(world, intents, content));

    REQUIRE(CountElementDrops(registry) == 2);
}

TEST_CASE("MiningSystem leaves an asteroid alone until it is tagged Destroyed", "[mining]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity asteroid = registry.create();
    registry.emplace<WorldTransform>(asteroid, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Asteroid>(asteroid);
    registry.emplace<AsteroidComposition>(asteroid, std::vector<ElementChance>{{"iron", 100}});

    mining_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(asteroid));
    CHECK(CountElementDrops(registry) == 0);
}

TEST_CASE("MiningSystem does not roll salvage for a destroyed rig hardpoint", "[mining]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Destroyed>(hardpoint);

    mining_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(hardpoint));
    CHECK(CountElementDrops(registry) == 0);
}
