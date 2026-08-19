#include <catch2/catch_test_macros.hpp>

#include "modes/space/ui/SystemMenu.h"

using sr::space::ui::system_menu::IsOpen;
using sr::space::ui::system_menu::QuitConfirmed;
using sr::space::ui::system_menu::SystemMenuState;
using sr::space::ui::system_menu::SystemMenuStateSingleton;

TEST_CASE("SystemMenu::IsOpen and QuitConfirmed are false with no singleton entity at all",
          "[system_menu]") {
    entt::registry registry;
    CHECK_FALSE(IsOpen(registry));
    CHECK_FALSE(QuitConfirmed(registry));
}

TEST_CASE("SystemMenu::IsOpen is false when the singleton exists but is closed", "[system_menu]") {
    entt::registry registry;
    const entt::entity singleton = registry.create();
    registry.emplace<SystemMenuStateSingleton>(singleton);
    registry.emplace<SystemMenuState>(singleton);

    CHECK_FALSE(IsOpen(registry));
    CHECK_FALSE(QuitConfirmed(registry));
}

TEST_CASE("SystemMenu::IsOpen and QuitConfirmed read an open, confirmed singleton",
          "[system_menu]") {
    entt::registry registry;
    const entt::entity singleton = registry.create();
    registry.emplace<SystemMenuStateSingleton>(singleton);
    SystemMenuState state;
    state.open = true;
    state.quitConfirmed = true;
    registry.emplace<SystemMenuState>(singleton, state);

    CHECK(IsOpen(registry));
    CHECK(QuitConfirmed(registry));
}
