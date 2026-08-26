#include <catch2/catch_test_macros.hpp>

#include "modes/space/ui/StorageMenu.h"
#include "shared/components/FlightOverlay.h"

using sr::FlightOverlayState;
using sr::FlightOverlayStateSingleton;
using sr::ItemKind;
using sr::ItemStack;
namespace storage_menu = sr::space::ui::storage_menu;

TEST_CASE("GroupedRows is empty for an empty stack list", "[storage-menu]") {
    CHECK(storage_menu::GroupedRows({}).empty());
}

TEST_CASE("GroupedRows groups elements under one header, modules under another, in that order",
          "[storage-menu]") {
    std::vector<ItemStack> stacks;
    stacks.push_back(ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 14.0f});
    stacks.push_back(ItemStack{ItemKind::Element, "Fe", 3, 2.0f});
    stacks.push_back(ItemStack{ItemKind::Element, "Ir", 8, 5.0f});

    const std::vector<storage_menu::GroupedEntry> rows = storage_menu::GroupedRows(stacks);

    REQUIRE(rows.size() == 5);  // "ELEMENTS", Fe, Ir, "MODULES", pulse_cannon_i.
    CHECK(rows[0].isHeader);
    CHECK(rows[0].headerLabel == "ELEMENTS");
    CHECK_FALSE(rows[1].isHeader);
    CHECK(rows[1].stack.id == "Fe");
    CHECK_FALSE(rows[2].isHeader);
    CHECK(rows[2].stack.id == "Ir");
    CHECK(rows[3].isHeader);
    CHECK(rows[3].headerLabel == "MODULES");
    CHECK_FALSE(rows[4].isHeader);
    CHECK(rows[4].stack.id == "pulse_cannon_i");
}

TEST_CASE("GroupedRows omits a header for a group with nothing in it", "[storage-menu]") {
    std::vector<ItemStack> stacks;
    stacks.push_back(ItemStack{ItemKind::Element, "Fe", 3, 2.0f});

    const std::vector<storage_menu::GroupedEntry> rows = storage_menu::GroupedRows(stacks);

    REQUIRE(rows.size() == 2);  // "ELEMENTS", Fe -- no empty "MODULES" heading.
    CHECK(rows[0].isHeader);
    CHECK(rows[0].headerLabel == "ELEMENTS");
    CHECK_FALSE(rows[1].isHeader);
    for (const storage_menu::GroupedEntry& row : rows) {
        CHECK(row.headerLabel != "MODULES");
    }
}

TEST_CASE("StorageMenu::IsOpen is false with no singleton entity at all", "[storage-menu]") {
    entt::registry registry;
    CHECK_FALSE(storage_menu::IsOpen(registry));
}

TEST_CASE("StorageMenu::IsOpen reads FlightOverlayState::inventoryOpen", "[storage-menu]") {
    entt::registry registry;
    const entt::entity singleton = registry.create();
    registry.emplace<FlightOverlayStateSingleton>(singleton);
    registry.emplace<FlightOverlayState>(singleton,
                                         FlightOverlayState{true, false, {}, entt::null});

    CHECK(storage_menu::IsOpen(registry));
}
