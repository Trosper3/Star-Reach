#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/ui/CockpitHud.h"
#include "shared/components/Health.h"
#include "shared/components/Rig.h"

using Catch::Approx;
using sr::Health;
using sr::Rig;
using sr::space::ui::cockpit_hud::AggregateHullFraction;

TEST_CASE("AggregateHullFraction is 0 for a rig with no children", "[cockpit-hud]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    CHECK(AggregateHullFraction(registry, root) == 0.0f);
}

TEST_CASE("AggregateHullFraction is 0 for a nonexistent rig", "[cockpit-hud]") {
    entt::registry registry;
    CHECK(AggregateHullFraction(registry, entt::null) == 0.0f);
}

TEST_CASE("AggregateHullFraction is 1 when every hardpoint is at full health", "[cockpit-hud]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;
    for (int i = 0; i < 3; ++i) {
        const entt::entity child = registry.create();
        registry.emplace<Health>(child, 50.0f, 50.0f);
        rig.children.push_back(child);
    }
    registry.emplace<Rig>(root, std::move(rig));

    CHECK(AggregateHullFraction(registry, root) == Approx(1.0f));
}

TEST_CASE("AggregateHullFraction sums current and max across all hardpoints", "[cockpit-hud]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;

    const entt::entity half = registry.create();
    registry.emplace<Health>(half, 25.0f, 50.0f);
    rig.children.push_back(half);

    const entt::entity dead = registry.create();
    registry.emplace<Health>(dead, 0.0f, 50.0f);
    rig.children.push_back(dead);

    registry.emplace<Rig>(root, std::move(rig));

    // (25 + 0) / (50 + 50) = 0.25
    CHECK(AggregateHullFraction(registry, root) == Approx(0.25f));
}
