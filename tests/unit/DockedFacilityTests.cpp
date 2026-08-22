#include <catch2/catch_test_macros.hpp>

#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/rig/DockedFacility.h"

using sr::Destroyed;
using sr::Docked;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::ParentRig;
using sr::PlayerLocation;
namespace docked_facility = sr::docked_facility;

namespace {

struct Fixture {
    entt::registry registry;
    entt::entity station = registry.create();
    entt::entity requester = registry.create();

    Fixture() { registry.emplace<Docked>(requester, station, entt::null); }

    entt::entity AddFacility(FacilityKind kind, int grade) {
        const entt::entity hardpoint = registry.create();
        registry.emplace<FacilityRef>(hardpoint, kind, grade);
        registry.emplace<ParentRig>(hardpoint, station);
        return hardpoint;
    }
};

}  // namespace

TEST_CASE("DockedFacility resolves the hardpoint PlayerLocation names", "[docked-facility]") {
    Fixture fx;
    const entt::entity bench = fx.AddFacility(FacilityKind::Engineering, 3);
    fx.registry.emplace<PlayerLocation>(bench, PlayerLocation{bench});

    CHECK(docked_facility::DockedFacility(fx.registry, fx.requester, FacilityKind::Engineering) ==
          bench);
}

TEST_CASE("DockedFacility picks the bench PlayerLocation names, not the first of that kind",
          "[docked-facility]") {
    Fixture fx;
    fx.AddFacility(FacilityKind::Engineering, 1);  // Created first -- must NOT win.
    const entt::entity second = fx.AddFacility(FacilityKind::Engineering, 5);
    fx.registry.emplace<PlayerLocation>(second, PlayerLocation{second});

    const entt::entity resolved =
        docked_facility::DockedFacility(fx.registry, fx.requester, FacilityKind::Engineering);
    REQUIRE(resolved == second);
    CHECK(fx.registry.get<FacilityRef>(resolved).grade == 5);
}

TEST_CASE("DockedFacility is null when the requester is not Docked", "[docked-facility]") {
    entt::registry registry;
    const entt::entity requester = registry.create();
    const entt::entity bench = registry.create();
    registry.emplace<FacilityRef>(bench, FacilityKind::Engineering, 1);
    registry.emplace<PlayerLocation>(bench, PlayerLocation{bench});

    CHECK((docked_facility::DockedFacility(registry, requester, FacilityKind::Engineering) ==
           entt::null));
}

TEST_CASE("DockedFacility is null when PlayerLocation names a different FacilityKind",
          "[docked-facility]") {
    Fixture fx;
    const entt::entity repairBay = fx.AddFacility(FacilityKind::Repair, 1);
    fx.registry.emplace<PlayerLocation>(repairBay, PlayerLocation{repairBay});

    CHECK((docked_facility::DockedFacility(fx.registry, fx.requester, FacilityKind::Engineering) ==
           entt::null));
}

TEST_CASE("DockedFacility is null when PlayerLocation names a Destroyed hardpoint",
          "[docked-facility]") {
    Fixture fx;
    const entt::entity bench = fx.AddFacility(FacilityKind::Engineering, 1);
    fx.registry.emplace<Destroyed>(bench);
    fx.registry.emplace<PlayerLocation>(bench, PlayerLocation{bench});

    CHECK((docked_facility::DockedFacility(fx.registry, fx.requester, FacilityKind::Engineering) ==
           entt::null));
}

TEST_CASE("DockedFacility is null when PlayerLocation names a bench at a different station",
          "[docked-facility]") {
    Fixture fx;
    const entt::entity otherStation = fx.registry.create();
    const entt::entity foreignBench = fx.registry.create();
    fx.registry.emplace<FacilityRef>(foreignBench, FacilityKind::Engineering, 1);
    fx.registry.emplace<ParentRig>(foreignBench, otherStation);
    fx.registry.emplace<PlayerLocation>(foreignBench, PlayerLocation{foreignBench});

    CHECK((docked_facility::DockedFacility(fx.registry, fx.requester, FacilityKind::Engineering) ==
           entt::null));
}
