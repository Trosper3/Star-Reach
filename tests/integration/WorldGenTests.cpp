#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/factories/WorldGen.h"
#include "modes/space/systems/OrbitSystem.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Mining.h"
#include "shared/components/Orbit.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"

using Catch::Approx;
using sr::Asteroid;
using sr::AsteroidComposition;
using sr::BodyKind;
using sr::GravityWell;
using sr::Health;
using sr::HitRadius;
using sr::Length;
using sr::OrbitBody;
using sr::PlayerControlled;
using sr::Rig;
using sr::Velocity;
using sr::WorldBody;
using sr::WorldTransform;
using sr::core::ContentLibrary;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace world_gen = sr::space::world_gen;
namespace orbit_system = sr::space::orbit_system;

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

}  // namespace

TEST_CASE("PopulateSystem seeds exactly one sun", "[world_gen]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    const auto view = world.Registry().view<GravityWell>();
    CHECK(std::distance(view.begin(), view.end()) == 1);
}

TEST_CASE("PopulateSystem seeds a plausible number of planets", "[world_gen]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    // Asteroids now carry OrbitBody too (architecture.md 12.28) -- exclude them so this stays a
    // planet count, not a "everything that orbits" count.
    const auto view = world.Registry().view<OrbitBody>(entt::exclude<Asteroid>);
    const auto count = std::distance(view.begin(), view.end());
    CHECK(count >= 2);
    CHECK(count <= 6);
}

TEST_CASE("PopulateSystem seeds exactly eight asteroids with rolled composition", "[world_gen]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    const auto view = registry.view<Asteroid, Health, AsteroidComposition>();
    CHECK(std::distance(view.begin(), view.end()) == 8);

    for (auto [entity, composition] : registry.view<AsteroidComposition>().each()) {
        (void)entity;
        CHECK_FALSE(composition.materials.empty());
        for (const auto& material : composition.materials) {
            CHECK(material.percent >= 1);
            CHECK(material.percent <= 100);
        }
    }
}

TEST_CASE("PopulateSystem seeds a plausible NPC presence with no PlayerControlled entity",
          "[world_gen]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    const auto view = registry.view<Rig>(entt::exclude<PlayerControlled>);
    const auto count = std::distance(view.begin(), view.end());
    CHECK(count >= 3);
    CHECK(count <= 5);
    CHECK(registry.view<PlayerControlled>().empty());
}

TEST_CASE("PopulateSystem emits a WorldBody of each of Star, Planet and Asteroid, and every "
          "entity with WorldTransform also carries WorldBody",
          "[world_gen]") {
    // The assertion that would have caught architecture.md 12.28 finding A: the sun, every
    // planet and every asteroid satisfied neither WorldRenderer view, so nothing drew them.
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    bool sawStar = false;
    bool sawPlanet = false;
    bool sawAsteroid = false;
    for (auto [entity, body] : registry.view<WorldBody>().each()) {
        (void)entity;
        sawStar |= body.kind == BodyKind::Star;
        sawPlanet |= body.kind == BodyKind::Planet;
        sawAsteroid |= body.kind == BodyKind::Asteroid;
    }
    CHECK(sawStar);
    CHECK(sawPlanet);
    CHECK(sawAsteroid);

    for (auto [entity, xf] : registry.view<WorldTransform>().each()) {
        (void)xf;
        // NPC rig roots and hardpoints carry WorldTransform too, but are excluded from WorldBody
        // by design (WorldRenderer.h): a rig is not one drawable. Restrict the blanket check to
        // non-rig bodies, which is exactly the set WorldGen itself spawns.
        if (!registry.all_of<Rig>(entity) && !registry.all_of<sr::ParentRig>(entity)) {
            CHECK(registry.all_of<WorldBody>(entity));
        }
    }
}

TEST_CASE("PopulateSystem's asteroids carry OrbitBody and HitRadius, not Velocity",
          "[world_gen]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    for (const entt::entity entity : registry.view<Asteroid>()) {
        CHECK(registry.all_of<OrbitBody>(entity));
        CHECK(registry.all_of<HitRadius>(entity));
        CHECK_FALSE(registry.all_of<Velocity>(entity));

        const auto& orbit = registry.get<OrbitBody>(entity);
        CHECK(orbit.radius >= 1800.0f);
        CHECK(orbit.radius <= 2800.0f);
    }
}

TEST_CASE("An asteroid at the belt's inner edge is still there after 10,000 ticks", "[world_gen]") {
    // Regression test for the OrbitBody-not-BodyMass decision (architecture.md 12.28): an
    // asteroid with Velocity and no damping inside the gravity well would accelerate unbounded
    // into the star and the belt would drain.
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    entt::entity innermost = entt::null;
    float innermostRadius = 1.0e9f;
    for (auto [entity, orbit] : registry.view<OrbitBody, Asteroid>().each()) {
        if (orbit.radius < innermostRadius) {
            innermostRadius = orbit.radius;
            innermost = entity;
        }
    }
    REQUIRE((innermost != entt::null));

    sr::core::IntentQueue intents;
    for (int tick = 0; tick < 10000; ++tick) {
        orbit_system::Tick(SystemContext{world, intents, content, 1.0f / 60.0f,
                                         static_cast<unsigned long long>(tick)});
    }

    const float distance = Length(registry.get<WorldTransform>(innermost).position);
    CHECK(distance == Approx(innermostRadius).margin(0.5f));
}

TEST_CASE("PopulateSystem is deterministic for a given seed", "[world_gen]") {
    const ContentLibrary content = Content();

    SystemWorld worldA("sol");
    world_gen::PopulateSystem(worldA, content, 1234u);
    SystemWorld worldB("sol");
    world_gen::PopulateSystem(worldB, content, 1234u);

    const auto countOf = [](SystemWorld& world) {
        entt::registry& registry = world.Registry();
        return std::make_tuple(registry.view<OrbitBody>().size(), registry.view<Asteroid>().size(),
                               registry.view<Rig>().size());
    };
    CHECK(countOf(worldA) == countOf(worldB));
}
