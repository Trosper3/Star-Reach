#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/render/WorldRenderer.h"
#include "shared/blueprints/Taxonomy.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"

using sr::Destroyed;
using sr::DrawLayer;
using sr::HitRadius;
using sr::LocalTransform;
using sr::PreviousTransform;
using sr::Propulsion;
using sr::ShellKind;
using sr::ShellRole;
using sr::Vec2;
using sr::WorldTransform;
using sr::space::render::HasVisiblePropulsion;
using sr::space::render::SortedHardpointsForDraw;

namespace {

// A minimal drawable hardpoint: every component DrawHardpoints/SortedHardpointsForDraw need,
// nothing DrawHardpoints doesn't read (WorldRendererTests doesn't touch raylib -- these tests
// exercise the pure sort, not the draw calls).
entt::entity MakeHardpoint(entt::registry& registry, ShellKind kind, int drawLayer, float localY) {
    const entt::entity entity = registry.create();
    registry.emplace<WorldTransform>(entity, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<PreviousTransform>(entity, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<ShellRole>(entity, kind);
    registry.emplace<HitRadius>(entity, 5.0f);
    registry.emplace<DrawLayer>(entity, drawLayer);
    registry.emplace<LocalTransform>(entity, Vec2{0.0f, localY}, 0.0f);
    return entity;
}

}  // namespace

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

TEST_CASE(
    "SortedHardpointsForDraw draws a ventral mount before a dorsal one, regardless of local y",
    "[world-renderer]") {
    entt::registry registry;
    // Local y is deliberately set backwards from what a y-only sort would want, so this only
    // passes if layer (features.md 3.5's five-layer stack) wins the comparison ahead of the y
    // tiebreak: the ventral engine has the HIGHER y, the dorsal turret the LOWER one.
    const entt::entity ventral = MakeHardpoint(registry, ShellKind::Engine, 1, 100.0f);
    const entt::entity dorsal = MakeHardpoint(registry, ShellKind::Weapon, 4, -100.0f);

    const std::vector<entt::entity> order = SortedHardpointsForDraw(registry);
    REQUIRE(order.size() == 2);
    // Later in draw order paints on top -- a dorsal turret must never be painted first and then
    // covered by the ventral engine drawn after it.
    CHECK(order.front() == ventral);
    CHECK(order.back() == dorsal);
}

TEST_CASE("SortedHardpointsForDraw tie-breaks equal layers by ascending local y",
          "[world-renderer]") {
    entt::registry registry;
    const entt::entity high = MakeHardpoint(registry, ShellKind::Armor, 2, 10.0f);
    const entt::entity low = MakeHardpoint(registry, ShellKind::Armor, 2, -10.0f);

    const std::vector<entt::entity> order = SortedHardpointsForDraw(registry);
    REQUIRE(order.size() == 2);
    CHECK(order.front() == low);
    CHECK(order.back() == high);
}

TEST_CASE("SortedHardpointsForDraw excludes a destroyed hardpoint", "[world-renderer]") {
    entt::registry registry;
    const entt::entity alive = MakeHardpoint(registry, ShellKind::Armor, 2, 0.0f);
    const entt::entity dead = MakeHardpoint(registry, ShellKind::Armor, 2, 1.0f);
    registry.emplace<Destroyed>(dead);

    const std::vector<entt::entity> order = SortedHardpointsForDraw(registry);
    REQUIRE(order.size() == 1);
    CHECK(order.front() == alive);
}
