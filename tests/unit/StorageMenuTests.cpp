#include <catch2/catch_test_macros.hpp>

#include "modes/space/ui/StorageMenu.h"

using sr::CargoHold;
using sr::MaterialStack;
using sr::ModuleId;
namespace storage_menu = sr::space::ui::storage_menu;

TEST_CASE("Rows lists modules before materials, materials with their quantity", "[storage-menu]") {
    CargoHold cargo;
    cargo.modules.push_back(ModuleId("pulse_cannon_i"));
    cargo.materials.push_back(MaterialStack{"Fe", 3});

    const std::vector<std::string> rows = storage_menu::Rows(cargo);

    REQUIRE(rows.size() == 2);
    CHECK(rows[0] == "pulse_cannon_i");
    CHECK(rows[1] == "Fe x3");
}

TEST_CASE("Rows is empty for an empty CargoHold", "[storage-menu]") {
    CHECK(storage_menu::Rows(CargoHold{}).empty());
}
