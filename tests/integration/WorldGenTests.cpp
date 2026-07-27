#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/registries/ContentLibrary.h"
#include "modes/space/factories/WorldGen.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Mining.h"
#include "shared/components/Orbit.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"

using sr::Asteroid;
using sr::AsteroidComposition;
using sr::GravityWell;
using sr::Health;
using sr::OrbitBody;
using sr::PlayerControlled;
using sr::Rig;
using sr::core::ContentLibrary;
using sr::space::SystemWorld;
namespace world_gen = sr::space::world_gen;

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

    const auto view = world.Registry().view<OrbitBody>();
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
