#include <catch2/catch_test_macros.hpp>

#include "modes/space/ui/BuildMenu.h"

using sr::CargoHold;
using sr::ItemKind;
using sr::ItemStack;
using sr::Wallet;
namespace build_menu = sr::space::ui::build_menu;

TEST_CASE("CanAfford is true only when the wallet covers the cost", "[build-menu]") {
    CHECK(build_menu::CanAfford(Wallet{500}, 500, CargoHold{}, {}));
    CHECK(build_menu::CanAfford(Wallet{600}, 500, CargoHold{}, {}));
    CHECK_FALSE(build_menu::CanAfford(Wallet{400}, 500, CargoHold{}, {}));
}

TEST_CASE("CanAfford also requires the cargo hold to cover the material cost", "[build-menu]") {
    CargoHold cargo;
    cargo.stacks.push_back(ItemStack{ItemKind::Element, "Fe", 10, 1.0f});
    const std::vector<ItemStack> materialCost{{ItemKind::Element, "Fe", 5, 0.0f}};

    CHECK(build_menu::CanAfford(Wallet{500}, 500, cargo, materialCost));
    CHECK_FALSE(build_menu::CanAfford(Wallet{400}, 500, cargo, materialCost));

    const std::vector<ItemStack> tooMuch{{ItemKind::Element, "Fe", 20, 0.0f}};
    CHECK_FALSE(build_menu::CanAfford(Wallet{500}, 500, cargo, tooMuch));

    const std::vector<ItemStack> notHeld{{ItemKind::Element, "Si", 1, 0.0f}};
    CHECK_FALSE(build_menu::CanAfford(Wallet{500}, 500, cargo, notHeld));
}

TEST_CASE("BuildStationBuildRequest/BuildPlaceShipRequest carry their fields through",
          "[build-menu]") {
    const auto station = build_menu::BuildStationBuildRequest(sr::BlueprintId("aegis_outpost"),
                                                              sr::Vec2{10.0f, 5.0f}, 1.5f, 500);
    CHECK(station.blueprint == sr::BlueprintId("aegis_outpost"));
    CHECK(station.position == sr::Vec2{10.0f, 5.0f});
    CHECK(station.rotation == 1.5f);
    CHECK(station.cost == 500);

    const auto ship = build_menu::BuildPlaceShipRequest(sr::BlueprintId("aegis_vanguard"),
                                                        sr::Vec2{1.0f, 2.0f}, 0.0f, 300);
    CHECK(ship.blueprint == sr::BlueprintId("aegis_vanguard"));
    CHECK(ship.cost == 300);
}
