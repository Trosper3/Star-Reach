#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/ui/BridgeView.h"
#include "shared/components/Facility.h"
#include "shared/components/Rig.h"

using sr::Destroyed;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::Rig;
using sr::space::ui::bridge_view::AvailableTabs;

TEST_CASE("AvailableTabs is empty for a rig with no FacilityRef hardpoints", "[bridge-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    CHECK(AvailableTabs(registry, root).empty());
}

TEST_CASE("AvailableTabs is empty for a nonexistent rig", "[bridge-view]") {
    entt::registry registry;
    CHECK(AvailableTabs(registry, entt::null).empty());
}

TEST_CASE("AvailableTabs lists tabs in FacilityKind declaration order, not discovery order",
          "[bridge-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;

    // Attach Storage before Repair -- the result should still read Repair, then Storage.
    const entt::entity storage = registry.create();
    registry.emplace<FacilityRef>(storage, FacilityKind::Storage);
    rig.children.push_back(storage);

    const entt::entity repair = registry.create();
    registry.emplace<FacilityRef>(repair, FacilityKind::Repair);
    rig.children.push_back(repair);

    registry.emplace<Rig>(root, std::move(rig));

    const auto tabs = AvailableTabs(registry, root);
    REQUIRE(tabs.size() == 2);
    CHECK(tabs[0] == FacilityKind::Repair);
    CHECK(tabs[1] == FacilityKind::Storage);
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

TEST_CASE("AvailableTabs de-duplicates two hardpoints of the same kind", "[bridge-view]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    Rig rig;

    for (int i = 0; i < 2; ++i) {
        const entt::entity repair = registry.create();
        registry.emplace<FacilityRef>(repair, FacilityKind::Repair);
        rig.children.push_back(repair);
    }

    registry.emplace<Rig>(root, std::move(rig));

    const auto tabs = AvailableTabs(registry, root);
    REQUIRE(tabs.size() == 1);
    CHECK(tabs[0] == FacilityKind::Repair);
}
