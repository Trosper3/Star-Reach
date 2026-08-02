#include <catch2/catch_test_macros.hpp>

#include "modes/space/ui/EngineerMenu.h"

using sr::ModuleId;
namespace engineer_menu = sr::space::ui::engineer_menu;

TEST_CASE("BuildMergeRequest carries both module ids through", "[engineer-menu]") {
    const auto request =
        engineer_menu::BuildMergeRequest(ModuleId("pulse_cannon_i"), ModuleId("autocannon_i"));
    CHECK(request.primary == ModuleId("pulse_cannon_i"));
    CHECK(request.secondary == ModuleId("autocannon_i"));
}
