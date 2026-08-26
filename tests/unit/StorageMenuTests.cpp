#include <catch2/catch_test_macros.hpp>

#include "modes/space/ui/StorageMenu.h"
#include "shared/components/FlightOverlay.h"

using sr::FlightOverlayState;
using sr::FlightOverlayStateSingleton;
using sr::ItemKind;
using sr::ItemStack;
namespace storage_menu = sr::space::ui::storage_menu;

TEST_CASE("Rows lists modules before elements, elements with their quantity", "[storage-menu]") {
    std::vector<ItemStack> stacks;
    stacks.push_back(ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 14.0f});
    stacks.push_back(ItemStack{ItemKind::Element, "Fe", 3, 2.0f});

    const std::vector<std::string> rows = storage_menu::Rows(stacks);

    REQUIRE(rows.size() == 2);
    CHECK(rows[0] == "pulse_cannon_i");
    CHECK(rows[1] == "Fe x3");
}

TEST_CASE("Rows is empty for an empty stack list", "[storage-menu]") {
    CHECK(storage_menu::Rows({}).empty());
}

TEST_CASE("OrderedStacks matches Rows' own iteration order -- modules before elements",
          "[storage-menu]") {
    std::vector<ItemStack> stacks;
    stacks.push_back(ItemStack{ItemKind::Element, "Fe", 3, 2.0f});
    stacks.push_back(ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 14.0f});

    const std::vector<ItemStack> ordered = storage_menu::OrderedStacks(stacks);

    REQUIRE(ordered.size() == 2);
    CHECK(ordered[0].kind == ItemKind::Module);
    CHECK(ordered[0].id == "pulse_cannon_i");
    CHECK(ordered[1].kind == ItemKind::Element);
    CHECK(ordered[1].id == "Fe");
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
