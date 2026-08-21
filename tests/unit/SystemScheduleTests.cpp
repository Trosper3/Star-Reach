#include <algorithm>
#include <array>

#include <catch2/catch_test_macros.hpp>

#include "modes/space/systems/SystemSchedule.h"

using sr::space::ScheduledSystem;
using sr::space::TickSchedule;

namespace {

// The test's own list, not a re-derivation of modes/space/systems/ -- a test that reads the
// directory it is checking can never fail (the issue's own membership rule). Count must match
// the number of *System.h files under modes/space/systems/, excluding System.h and
// SystemSchedule.h itself.
constexpr std::array<std::string_view, 34> kExpectedSystems{
    "PlayerLocationSystem", "WarpSystem",             "ConstructionSystem", "ModuleEquipSystem",
    "PowerSystem",          "SpawnSystem",            "OrbitSystem",        "PlayerInputSystem",
    "PhysicsSystem",        "HierarchySystem",        "HazardSystem",       "DockingSystem",
    "EngineerSystem",       "RefactorSystem",         "StationServicesSystem", "TargetingSystem",
    "NpcAiSystem",          "WeaponSystem",           "CollisionSystem",    "ProjectileSystem",
    "PartySystem",          "DamageSystem",           "CaptureSystem",      "TutorialSystem",
    "MiningSystem",         "ContractSystem",         "DistressSystem",     "LootSystem",
    "CommsSystem",          "FactionEconomySystem",   "DiscoverySystem",    "CommanderSystem",
    "ResearchSystem",       "TemplateMarketSystem",
};

std::size_t IndexOf(std::string_view name) {
    const auto& schedule = TickSchedule();
    const auto it = std::find_if(schedule.begin(), schedule.end(),
                                 [name](const ScheduledSystem& s) { return s.name == name; });
    REQUIRE(it != schedule.end());
    return static_cast<std::size_t>(it - schedule.begin());
}

}  // namespace

TEST_CASE("Every expected system appears in the schedule exactly once", "[schedule]") {
    const auto& schedule = TickSchedule();
    REQUIRE(schedule.size() == kExpectedSystems.size());

    for (std::string_view expected : kExpectedSystems) {
        const auto count =
            std::count_if(schedule.begin(), schedule.end(),
                          [expected](const ScheduledSystem& s) { return s.name == expected; });
        CHECK(count == 1);
    }
}

// P0-04 (architecture.md 13.3 finding G): a hardpoint's WorldTransform is derived from its
// root's, which only reaches this tick's pose once PhysicsSystem has moved it.
TEST_CASE("HierarchySystem runs after PhysicsSystem", "[schedule]") {
    CHECK(IndexOf("PhysicsSystem") < IndexOf("HierarchySystem"));
}

// DockingSystem's range check must read each hardpoint's settled-this-tick position.
TEST_CASE("HierarchySystem runs before DockingSystem", "[schedule]") {
    CHECK(IndexOf("HierarchySystem") < IndexOf("DockingSystem"));
}

// WeaponSystem spawns the projectiles ProjectileSystem resolves and is the common upstream
// producer for both combat-resolution systems that feed DamageSystem's PendingDamage drain.
TEST_CASE("WeaponSystem runs before ProjectileSystem and CollisionSystem", "[schedule]") {
    CHECK(IndexOf("WeaponSystem") < IndexOf("ProjectileSystem"));
    CHECK(IndexOf("WeaponSystem") < IndexOf("CollisionSystem"));
}

// DamageSystem is the tick's final word on destruction (SystemSchedule.h rule 5); both
// combat-resolution systems must have queued their PendingDamage before it drains.
TEST_CASE("ProjectileSystem and CollisionSystem run before DamageSystem", "[schedule]") {
    CHECK(IndexOf("ProjectileSystem") < IndexOf("DamageSystem"));
    CHECK(IndexOf("CollisionSystem") < IndexOf("DamageSystem"));
}

// architecture.md 12.24 step 2: PlayerInputSystem writes the ThrustInput PhysicsSystem
// integrates and the FireIntent WeaponSystem drains this same tick, and must run before
// DockingSystem zeroes ThrustInput for a Docked rig -- the constraint that would silently
// break (a docked player would fly away) if this ran after it instead.
TEST_CASE("PlayerInputSystem runs before PhysicsSystem, WeaponSystem, and DockingSystem",
          "[schedule]") {
    CHECK(IndexOf("PlayerInputSystem") < IndexOf("PhysicsSystem"));
    CHECK(IndexOf("PlayerInputSystem") < IndexOf("WeaponSystem"));
    CHECK(IndexOf("PlayerInputSystem") < IndexOf("DockingSystem"));
}
