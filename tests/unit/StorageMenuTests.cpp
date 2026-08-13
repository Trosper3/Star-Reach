#include <catch2/catch_test_macros.hpp>

#include "modes/space/ui/StorageMenu.h"

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
