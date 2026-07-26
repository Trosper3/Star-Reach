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
