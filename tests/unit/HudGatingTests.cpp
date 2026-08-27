#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/ui/HudGating.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"

using sr::CrewRating;
using sr::Destroyed;
using sr::Rig;
using sr::SensorRange;
using sr::ShellKind;
using sr::ShellRole;
using sr::space::ui::hud_gating::BuildSlots;
using sr::space::ui::hud_gating::HudSurface;
using sr::space::ui::hud_gating::IsOnline;
using sr::space::ui::hud_gating::kSurfaceOrder;
using sr::space::ui::hud_gating::Label;

TEST_CASE("IsOnline is false for entt::null regardless of surface", "[hud-gating]") {
    entt::registry registry;
    CHECK_FALSE(IsOnline(registry, entt::null, HudSurface::Sensor));
    CHECK_FALSE(IsOnline(registry, entt::null, HudSurface::Command));
}

TEST_CASE("Sensor is online exactly when SensorRange::units is positive", "[hud-gating]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<SensorRange>(root, 500.0f);
    CHECK(IsOnline(registry, root, HudSurface::Sensor));

    // Mirrors shared/rig/ModuleAttachment.cpp's RecomputeRigTotals zeroing SensorRange::units
    // once the last living Sensor hardpoint dies -- the component stays present, just at 0.
    registry.get<SensorRange>(root).units = 0.0f;
    CHECK_FALSE(IsOnline(registry, root, HudSurface::Sensor));
}

TEST_CASE("Sensor is offline with no SensorRange component at all", "[hud-gating]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    CHECK_FALSE(IsOnline(registry, root, HudSurface::Sensor));
}

TEST_CASE("Command is online with a living control-shell CrewRating rolled into command",
          "[hud-gating]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity cockpit = registry.create();
    registry.emplace<CrewRating>(cockpit, 0.0f, 0.4f, 0.0f, 0.0f);
    registry.emplace<ShellRole>(cockpit, ShellKind::Chassis);
    rig.children.push_back(cockpit);
    registry.emplace<Rig>(root, std::move(rig));

    CHECK(IsOnline(registry, root, HudSurface::Command));
}

TEST_CASE("Command is offline once the crewed hardpoint is Destroyed", "[hud-gating]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity cockpit = registry.create();
    registry.emplace<CrewRating>(cockpit, 0.0f, 0.4f, 0.0f, 0.0f);
    registry.emplace<ShellRole>(cockpit, ShellKind::Chassis);
    registry.emplace<Destroyed>(cockpit);
    rig.children.push_back(cockpit);
    registry.emplace<Rig>(root, std::move(rig));

    CHECK_FALSE(IsOnline(registry, root, HudSurface::Command));
}

TEST_CASE("Command is offline for a zero command roll", "[hud-gating]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity cockpit = registry.create();
    registry.emplace<CrewRating>(cockpit, 0.5f, 0.0f, 0.0f, 0.0f);
    registry.emplace<ShellRole>(cockpit, ShellKind::Chassis);
    rig.children.push_back(cockpit);
    registry.emplace<Rig>(root, std::move(rig));

    CHECK_FALSE(IsOnline(registry, root, HudSurface::Command));
}

TEST_CASE("Command ignores a turret's own crew", "[hud-gating]") {
    // features.md 2.7: a turret's Crew is scoped to that hardpoint alone, never the rig's command
    // surface -- the same exclusion RecomputeRigTotals's own `crewed` walk applies.
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;
    const entt::entity turret = registry.create();
    registry.emplace<CrewRating>(turret, 0.0f, 0.9f, 0.0f, 0.0f);
    registry.emplace<ShellRole>(turret, ShellKind::Weapon);
    rig.children.push_back(turret);
    registry.emplace<Rig>(root, std::move(rig));

    CHECK_FALSE(IsOnline(registry, root, HudSurface::Command));
}

TEST_CASE("Comms, Construction, and Hyperdrive are always offline today", "[hud-gating]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<SensorRange>(root, 500.0f);
    Rig rig;
    const entt::entity cockpit = registry.create();
    registry.emplace<CrewRating>(cockpit, 0.0f, 0.4f, 0.0f, 0.0f);
    registry.emplace<ShellRole>(cockpit, ShellKind::Chassis);
    rig.children.push_back(cockpit);
    registry.emplace<Rig>(root, std::move(rig));

    CHECK_FALSE(IsOnline(registry, root, HudSurface::Comms));
    CHECK_FALSE(IsOnline(registry, root, HudSurface::Construction));
    CHECK_FALSE(IsOnline(registry, root, HudSurface::Hyperdrive));
}

TEST_CASE("Label names every surface", "[hud-gating]") {
    CHECK(Label(HudSurface::Sensor) == "SENSORS");
    CHECK(Label(HudSurface::Comms) == "COMMS");
    CHECK(Label(HudSurface::Command) == "COMMAND");
    CHECK(Label(HudSurface::Construction) == "CONSTRUCTION");
    CHECK(Label(HudSurface::Hyperdrive) == "HYPERDRIVE");
}

TEST_CASE("BuildSlots returns exactly one slot per kSurfaceOrder entry, in that fixed order",
          "[hud-gating]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<SensorRange>(root, 500.0f);

    const auto slots = BuildSlots(registry, root);
    REQUIRE(slots.size() == kSurfaceOrder.size());
    for (std::size_t i = 0; i < kSurfaceOrder.size(); ++i) {
        CHECK(slots[i].surface == kSurfaceOrder[i]);
    }
    CHECK(slots[0].online);  // Sensor, from the SensorRange emplaced above.
}

TEST_CASE("BuildSlots keeps slot order fixed whether or not the rig exists at all",
          "[hud-gating]") {
    entt::registry registry;
    const auto slots = BuildSlots(registry, entt::null);
    REQUIRE(slots.size() == kSurfaceOrder.size());
    for (const auto& slot : slots) {
        CHECK_FALSE(slot.online);
    }
}
