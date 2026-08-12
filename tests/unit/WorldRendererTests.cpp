#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/render/WorldRenderer.h"
#include "shared/components/Physics.h"

using sr::Propulsion;
using sr::space::render::HasVisiblePropulsion;

TEST_CASE("HasVisiblePropulsion is false for a rig root with no Propulsion component",
          "[world-renderer]") {
    // A station (RigFactory.cpp: "stations get no Propulsion at all, rather than a zeroed one").
    entt::registry registry;
    const entt::entity station = registry.create();
    CHECK_FALSE(HasVisiblePropulsion(registry, station));
}

TEST_CASE("HasVisiblePropulsion is false for a rig root with a zeroed Propulsion",
          "[world-renderer]") {
    // A mobile hull whose last engine hardpoint has died.
    entt::registry registry;
    const entt::entity hulk = registry.create();
    registry.emplace<Propulsion>(hulk);
    CHECK_FALSE(HasVisiblePropulsion(registry, hulk));
}

TEST_CASE("HasVisiblePropulsion is true for a rig root with non-zero thrust", "[world-renderer]") {
    entt::registry registry;
    const entt::entity ship = registry.create();
    registry.emplace<Propulsion>(ship, 1000.0f, 50.0f, 300.0f);
    CHECK(HasVisiblePropulsion(registry, ship));
}
