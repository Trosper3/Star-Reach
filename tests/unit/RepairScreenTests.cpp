#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/ui/RepairScreen.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"

using sr::Destroyed;
using sr::Docked;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::FactionId;
using sr::FactionRef;
using sr::Health;
using sr::Rig;
using sr::space::ui::repair_screen::FacilityGrade;
using sr::space::ui::repair_screen::OwnedVesselAt;
using sr::space::ui::repair_screen::RepairAllActive;
using sr::space::ui::repair_screen::Rows;

TEST_CASE("Rows sorts hardpoints by integrity ascending", "[repair-screen]") {
    entt::registry registry;
    const entt::entity subject = registry.create();

    const entt::entity healthy = registry.create();
    registry.emplace<Health>(healthy, 90.0f, 100.0f);
    const entt::entity damaged = registry.create();
    registry.emplace<Health>(damaged, 10.0f, 100.0f);

    registry.emplace<Rig>(subject, std::vector<entt::entity>{healthy, damaged});

    const auto rows = Rows(registry, subject, /*hasOrder=*/false, entt::null, /*costPerHp=*/2);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].hardpoint == damaged);
    CHECK(rows[1].hardpoint == healthy);
}

TEST_CASE("Rows marks a Destroyed hardpoint disabled and never ordered", "[repair-screen]") {
    entt::registry registry;
    const entt::entity subject = registry.create();
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 0.0f, 100.0f);
    registry.emplace<Destroyed>(hardpoint);
    registry.emplace<Rig>(subject, std::vector<entt::entity>{hardpoint});

    const auto rows =
        Rows(registry, subject, /*hasOrder=*/true, /*orderedHardpoint=*/entt::null, 2);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].destroyed);
    CHECK_FALSE(rows[0].ordered);  // Repair All is active, but a destroyed row never is.
    CHECK(rows[0].row.style.disabled);
    CHECK(rows[0].row.value.find("REBUILD") != std::string::npos);
}

TEST_CASE("Rows marks exactly the hardpoint a single-target order names as ordered",
          "[repair-screen]") {
    entt::registry registry;
    const entt::entity subject = registry.create();
    const entt::entity targeted = registry.create();
    registry.emplace<Health>(targeted, 50.0f, 100.0f);
    const entt::entity other = registry.create();
    registry.emplace<Health>(other, 50.0f, 100.0f);
    registry.emplace<Rig>(subject, std::vector<entt::entity>{targeted, other});

    const auto rows = Rows(registry, subject, /*hasOrder=*/true, targeted, 2);
    REQUIRE(rows.size() == 2);
    for (const auto& row : rows) {
        CHECK(row.ordered == (row.hardpoint == targeted));
    }
}

TEST_CASE("Rows marks every living hardpoint ordered under a whole-rig order", "[repair-screen]") {
    entt::registry registry;
    const entt::entity subject = registry.create();
    const entt::entity a = registry.create();
    registry.emplace<Health>(a, 50.0f, 100.0f);
    const entt::entity b = registry.create();
    registry.emplace<Health>(b, 30.0f, 100.0f);
    registry.emplace<Rig>(subject, std::vector<entt::entity>{a, b});

    const auto rows = Rows(registry, subject, /*hasOrder=*/true, entt::null, 2);
    REQUIRE(rows.size() == 2);
    CHECK(rows[0].ordered);
    CHECK(rows[1].ordered);
}

TEST_CASE("Rows prices the cost to bring a hardpoint to full", "[repair-screen]") {
    entt::registry registry;
    const entt::entity subject = registry.create();
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);  // 50 missing
    registry.emplace<Rig>(subject, std::vector<entt::entity>{hardpoint});

    const auto rows = Rows(registry, subject, /*hasOrder=*/false, entt::null, /*costPerHp=*/3);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].row.value.find("150cr") != std::string::npos);  // 50 missing * 3
}

TEST_CASE("RepairAllActive is true only for a whole-rig order", "[repair-screen]") {
    CHECK(RepairAllActive(/*hasOrder=*/true, /*orderedHardpoint=*/entt::null));
    CHECK_FALSE(RepairAllActive(true, entt::entity{7}));
    CHECK_FALSE(RepairAllActive(false, entt::null));
}

TEST_CASE("FacilityGrade reads FacilityRef::grade, defaulting to 1", "[repair-screen]") {
    entt::registry registry;
    const entt::entity graded = registry.create();
    registry.emplace<FacilityRef>(graded, FacilityKind::Repair, 3);
    CHECK(FacilityGrade(registry, graded) == 3);

    const entt::entity ungraded = registry.create();
    CHECK(FacilityGrade(registry, ungraded) == 1);
}

TEST_CASE("OwnedVesselAt finds the player's own docked vessel", "[repair-screen]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, entt::null);
    registry.emplace<FactionRef>(vessel, FactionId("aegis"));

    CHECK(OwnedVesselAt(registry, station, FactionId("aegis")) == vessel);
}

TEST_CASE("OwnedVesselAt returns null with no owned vessel docked at the station",
          "[repair-screen]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, entt::null);
    registry.emplace<FactionRef>(vessel, FactionId("kore"));

    CHECK((OwnedVesselAt(registry, station, FactionId("aegis")) == entt::null));
}
