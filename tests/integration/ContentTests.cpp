#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/registries/ContentLibrary.h"

using sr::core::ContentLibrary;
using sr::core::LoadReport;

namespace {

ContentLibrary LoadShippedContent(LoadReport& report) {
    ContentLibrary library;
    report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    return library;
}

}  // namespace

TEST_CASE("The shipped content set loads without errors", "[content]") {
    LoadReport report;
    const ContentLibrary library = LoadShippedContent(report);

    INFO(report.Summary());
    REQUIRE(report.ok());
    CHECK(library.ShellCount() > 0);
    CHECK(library.ModuleCount() > 0);
    CHECK(library.ShipCount() > 0);
}

TEST_CASE("Every shipped blueprint is instantiable", "[content]") {
    // This is the test that makes Law 10 pay for itself. Authored data that cannot become a
    // live rig fails CI here rather than failing in a player's session, and it does so without
    // anyone remembering to check -- which is the entire difference between this and a
    // documented convention.
    LoadReport load;
    const ContentLibrary library = LoadShippedContent(load);
    REQUIRE(load.ok());

    const LoadReport validation = library.ValidateAll();
    INFO(validation.Summary());
    CHECK(validation.ok());
}

TEST_CASE("Blueprint ids resolve to the ships they name", "[content]") {
    LoadReport report;
    const ContentLibrary library = LoadShippedContent(report);
    REQUIRE(report.ok());

    for (const auto& id : library.ShipIds()) {
        INFO("blueprint: " << id.str());
        const auto* ship = library.FindShip(id);
        REQUIRE(ship != nullptr);
        CHECK(ship->id == id);
        CHECK_FALSE(ship->rig.mounts.empty());
    }
}

TEST_CASE("An unknown id resolves to nullptr rather than a default", "[content]") {
    LoadReport report;
    const ContentLibrary library = LoadShippedContent(report);

    CHECK(library.FindShip(sr::BlueprintId("no_such_ship")) == nullptr);
    CHECK(library.FindShell(sr::ShellId("no_such_shell")) == nullptr);
    CHECK(library.FindModule(sr::ModuleId("no_such_module")) == nullptr);
}

TEST_CASE("RegisterCraftedModule makes a runtime-built module resolvable via FindModule",
          "[content]") {
    LoadReport report;
    ContentLibrary library = LoadShippedContent(report);
    REQUIRE(report.ok());

    sr::ModuleDef crafted;
    crafted.id = sr::ModuleId("player_crafted_one");
    crafted.kind = sr::ModuleKind::Weapon;
    crafted.weapon.damage = 42.0f;
    library.RegisterCraftedModule(crafted);

    const sr::ModuleDef* found = library.FindModule(sr::ModuleId("player_crafted_one"));
    REQUIRE(found != nullptr);
    CHECK(found->weapon.damage == 42.0f);
}

TEST_CASE("A crafted module id takes priority over an authored id of the same name", "[content]") {
    LoadReport report;
    ContentLibrary library = LoadShippedContent(report);
    REQUIRE(report.ok());

    sr::ModuleDef crafted;
    crafted.id = sr::ModuleId("pulse_cannon_i");
    crafted.kind = sr::ModuleKind::Weapon;
    crafted.weapon.damage = 999.0f;
    library.RegisterCraftedModule(crafted);

    const sr::ModuleDef* found = library.FindModule(sr::ModuleId("pulse_cannon_i"));
    REQUIRE(found != nullptr);
    CHECK(found->weapon.damage == 999.0f);
}

TEST_CASE("A crafted module survives reloading the authored content set", "[content]") {
    LoadReport report;
    ContentLibrary library = LoadShippedContent(report);
    REQUIRE(report.ok());

    sr::ModuleDef crafted;
    crafted.id = sr::ModuleId("player_crafted_two");
    crafted.kind = sr::ModuleKind::Armor;
    library.RegisterCraftedModule(crafted);

    const auto reload = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(reload.ok());

    CHECK(library.FindModule(sr::ModuleId("player_crafted_two")) != nullptr);
}
