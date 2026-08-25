#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

#include <entt/entity/registry.hpp>

#include "modes/space/ui/BridgeView.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"

using sr::CargoHold;
using sr::Destroyed;
using sr::Docked;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::ParentRig;
using sr::PlayerLocation;
using sr::Rig;
using sr::space::ui::bridge_view::AvailableTabs;
using sr::space::ui::bridge_view::BridgeTab;
using sr::space::ui::bridge_view::DockedStation;
using sr::space::ui::bridge_view::ScreenId;
using sr::space::ui::bridge_view::SelectTab;

TEST_CASE("AvailableTabs is empty for a rig with no FacilityRef hardpoints and no CargoHold",
          "[bridge-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    CHECK(AvailableTabs(registry, root).empty());
}

TEST_CASE("AvailableTabs is empty for a nonexistent rig", "[bridge-view]") {
    entt::registry registry;
    CHECK(AvailableTabs(registry, entt::null).empty());
}

TEST_CASE("AvailableTabs lists tabs in ScreenId declaration order, not discovery order",
          "[bridge-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;

    // Attach Repair before Docking -- the result should still read Bay, then Repair (ScreenId
    // order), not discovery order.
    const entt::entity repair = registry.create();
    registry.emplace<FacilityRef>(repair, FacilityKind::Repair);
    rig.children.push_back(repair);

    const entt::entity docking = registry.create();
    registry.emplace<FacilityRef>(docking, FacilityKind::Docking);
    rig.children.push_back(docking);

    registry.emplace<Rig>(root, std::move(rig));

    const auto tabs = AvailableTabs(registry, root);
    REQUIRE(tabs.size() == 2);
    CHECK(tabs[0].screen == ScreenId::Bay);
    CHECK(tabs[0].hardpoint == docking);
    CHECK(tabs[1].screen == ScreenId::Repair);
    CHECK(tabs[1].hardpoint == repair);
}

TEST_CASE("AvailableTabs excludes a Destroyed facility hardpoint", "[bridge-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;

    const entt::entity repair = registry.create();
    registry.emplace<FacilityRef>(repair, FacilityKind::Repair);
    registry.emplace<Destroyed>(repair);
    rig.children.push_back(repair);

    registry.emplace<Rig>(root, std::move(rig));

    CHECK(AvailableTabs(registry, root).empty());
}

TEST_CASE("AvailableTabs de-duplicates two hardpoints of the same kind, keeping the first",
          "[bridge-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;

    entt::entity firstRepair = entt::null;
    for (int i = 0; i < 2; ++i) {
        const entt::entity repair = registry.create();
        registry.emplace<FacilityRef>(repair, FacilityKind::Repair);
        rig.children.push_back(repair);
        if (i == 0) {
            firstRepair = repair;
        }
    }

    registry.emplace<Rig>(root, std::move(rig));

    const auto tabs = AvailableTabs(registry, root);
    REQUIRE(tabs.size() == 1);
    CHECK(tabs[0].screen == ScreenId::Repair);
    CHECK(tabs[0].hardpoint == firstRepair);
}

TEST_CASE(
    "AvailableTabs returns only the shipped screens when every facility kind and CargoHold live",
    "[bridge-view]") {
    // architecture.md 12.30's shipped-table fix: Market and Manufacturing have no screen behind
    // them yet, so they must not appear even though their FacilityKind hardpoints are living.
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;

    constexpr FacilityKind kAllKinds[] = {
        FacilityKind::Repair,  FacilityKind::Manufacturing, FacilityKind::Research,
        FacilityKind::Docking, FacilityKind::Trade,         FacilityKind::Engineering,
    };
    for (FacilityKind kind : kAllKinds) {
        const entt::entity hardpoint = registry.create();
        registry.emplace<FacilityRef>(hardpoint, kind);
        rig.children.push_back(hardpoint);
    }

    const entt::entity cargoBay = registry.create();
    registry.emplace<CargoHold>(cargoBay);
    rig.children.push_back(cargoBay);

    registry.emplace<Rig>(root, std::move(rig));

    const auto tabs = AvailableTabs(registry, root);
    REQUIRE(tabs.size() == 5);
    constexpr ScreenId kExpectedOrder[] = {
        ScreenId::Bay,         ScreenId::Storage,  ScreenId::Repair,
        ScreenId::Engineering, ScreenId::Research,
    };
    for (std::size_t i = 0; i < 5; ++i) {
        CHECK(tabs[i].screen == kExpectedOrder[i]);
    }
    // The Storage tab names no hardpoint -- there is nothing physical to move PlayerLocation onto.
    const auto storageTab = std::find_if(tabs.begin(), tabs.end(), [](const BridgeTab& tab) {
        return tab.screen == ScreenId::Storage;
    });
    REQUIRE(storageTab != tabs.end());
    CHECK((storageTab->hardpoint == entt::null));
}

TEST_CASE("AvailableTabs shows no Market tab for a living Trade hardpoint -- unshipped screen",
          "[bridge-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;

    const entt::entity trade = registry.create();
    registry.emplace<FacilityRef>(trade, FacilityKind::Trade);
    rig.children.push_back(trade);

    registry.emplace<Rig>(root, std::move(rig));

    CHECK(AvailableTabs(registry, root).empty());
}

TEST_CASE(
    "AvailableTabs shows no Manufacturing tab for a living Manufacturing hardpoint -- unshipped "
    "screen",
    "[bridge-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;

    const entt::entity manufacturing = registry.create();
    registry.emplace<FacilityRef>(manufacturing, FacilityKind::Manufacturing);
    rig.children.push_back(manufacturing);

    registry.emplace<Rig>(root, std::move(rig));

    CHECK(AvailableTabs(registry, root).empty());
}

TEST_CASE("AvailableTabs adds a Storage tab for a CargoHold with no Trade facility",
          "[bridge-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;

    const entt::entity cargoBay = registry.create();
    registry.emplace<CargoHold>(cargoBay);
    rig.children.push_back(cargoBay);

    registry.emplace<Rig>(root, std::move(rig));

    const auto tabs = AvailableTabs(registry, root);
    REQUIRE(tabs.size() == 1);
    CHECK(tabs[0].screen == ScreenId::Storage);
    CHECK((tabs[0].hardpoint == entt::null));
}

TEST_CASE("AvailableTabs excludes a Destroyed cargo bay from the Storage tab", "[bridge-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;

    const entt::entity cargoBay = registry.create();
    registry.emplace<CargoHold>(cargoBay);
    registry.emplace<Destroyed>(cargoBay);
    rig.children.push_back(cargoBay);

    registry.emplace<Rig>(root, std::move(rig));

    CHECK(AvailableTabs(registry, root).empty());
}

TEST_CASE("SelectTab moves PlayerLocation to the selected tab's hardpoint", "[bridge-view]") {
    entt::registry registry;
    const entt::entity shell = registry.create();
    registry.emplace<PlayerLocation>(shell, PlayerLocation{shell});

    const entt::entity repairHardpoint = registry.create();
    const std::vector<BridgeTab> tabs = {{ScreenId::Repair, repairHardpoint}};

    SelectTab(registry, shell, tabs, 0);

    CHECK_FALSE(registry.all_of<PlayerLocation>(shell));
    REQUIRE(registry.all_of<PlayerLocation>(repairHardpoint));
    CHECK(registry.get<PlayerLocation>(repairHardpoint).shell == repairHardpoint);
}

TEST_CASE("SelectTab does nothing for the Storage tab -- no hardpoint to move onto",
          "[bridge-view]") {
    entt::registry registry;
    const entt::entity shell = registry.create();
    registry.emplace<PlayerLocation>(shell, PlayerLocation{shell});

    const std::vector<BridgeTab> tabs = {{ScreenId::Storage, entt::null}};
    SelectTab(registry, shell, tabs, 0);

    REQUIRE(registry.all_of<PlayerLocation>(shell));
    CHECK(registry.get<PlayerLocation>(shell).shell == shell);
}

TEST_CASE("SelectTab does nothing for an out-of-range index", "[bridge-view]") {
    entt::registry registry;
    const entt::entity shell = registry.create();
    registry.emplace<PlayerLocation>(shell, PlayerLocation{shell});

    const std::vector<BridgeTab> tabs = {{ScreenId::Repair, registry.create()}};
    SelectTab(registry, shell, tabs, 5);

    REQUIRE(registry.all_of<PlayerLocation>(shell));
}

TEST_CASE("DockedStation is null while flying (no Docked, no facility hardpoint)",
          "[bridge-view]") {
    entt::registry registry;
    const entt::entity shell = registry.create();
    registry.emplace<PlayerLocation>(shell, PlayerLocation{shell});

    CHECK((DockedStation(registry) == entt::null));
}

TEST_CASE("DockedStation resolves via Docked while PlayerLocation is still on the arriving vessel",
          "[bridge-view]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity vessel = registry.create();
    registry.emplace<Docked>(vessel, station, entt::null);
    registry.emplace<PlayerLocation>(vessel, PlayerLocation{vessel});

    CHECK(DockedStation(registry) == station);
}

TEST_CASE(
    "DockedStation resolves via ParentRig once PlayerLocation moves onto a facility "
    "hardpoint",
    "[bridge-view]") {
    // architecture.md 12.30.1: PlayerControlled while standing in a facility is the station
    // itself, which never carries Docked -- only a visiting vessel does. DockedStation must keep
    // resolving anyway, or the router's tab strip goes dark the moment a screen is entered.
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity hardpoint = registry.create();
    registry.emplace<ParentRig>(hardpoint, station);
    registry.emplace<FacilityRef>(hardpoint, FacilityKind::Repair);
    registry.emplace<PlayerLocation>(hardpoint, PlayerLocation{hardpoint});

    CHECK(DockedStation(registry) == station);
}

TEST_CASE(
    "DockedStation resolves after boarding a different owned hull still docked at the "
    "station",
    "[bridge-view]") {
    entt::registry registry;
    const entt::entity station = registry.create();
    const entt::entity boardedHull = registry.create();
    registry.emplace<Docked>(boardedHull, station, entt::null);
    registry.emplace<PlayerLocation>(boardedHull, PlayerLocation{boardedHull});

    CHECK(DockedStation(registry) == station);
}

TEST_CASE("DockedStation is null with no PlayerLocation entity", "[bridge-view]") {
    entt::registry registry;
    CHECK((DockedStation(registry) == entt::null));
}
