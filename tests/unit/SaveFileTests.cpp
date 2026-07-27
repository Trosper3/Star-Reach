#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/serialization/SaveFile.h"

using sr::ShipBlueprint;
using sr::core::serialization::LoadShipBlueprint;
using sr::core::serialization::SaveShipBlueprint;

namespace {

ShipBlueprint MakeBlueprint() {
    ShipBlueprint bp;
    bp.id = sr::BlueprintId("aegis_vanguard");
    bp.schemaVersion = sr::kBlueprintSchemaVersion;
    bp.displayName = "Aegis Vanguard";
    bp.faction = sr::FactionId("aegis");
    bp.mobile = true;
    bp.structuralMassLimit = 250.0f;

    sr::MountBlueprint root;
    root.id = sr::MountId("root");
    root.shell = sr::ShellId("chassis");
    bp.rig.mounts.push_back(root);
    return bp;
}

// A fresh path per test, under the OS temp directory -- Catch2 runs test cases in one process
// sequentially by default, but a distinct path per test removes any doubt if that ever changes.
std::filesystem::path TempSavePath(const char* name) {
    return std::filesystem::temp_directory_path() / name;
}

}  // namespace

TEST_CASE("SaveShipBlueprint then LoadShipBlueprint round-trips the blueprint", "[save-file]") {
    const std::filesystem::path path = TempSavePath("sr_save_file_roundtrip.sav");
    const ShipBlueprint original = MakeBlueprint();

    REQUIRE(SaveShipBlueprint(path, original));
    const auto loaded = LoadShipBlueprint(path);

    REQUIRE(loaded.has_value());
    CHECK(loaded->id.str() == original.id.str());
    CHECK(loaded->schemaVersion == original.schemaVersion);
    CHECK(loaded->displayName == original.displayName);
    CHECK(loaded->rig.mounts.size() == original.rig.mounts.size());

    std::filesystem::remove(path);
}

TEST_CASE("LoadShipBlueprint returns nullopt for a nonexistent file", "[save-file]") {
    const std::filesystem::path path = TempSavePath("sr_save_file_does_not_exist.sav");
    std::filesystem::remove(path);  // In case a previous failed run left it behind.
    CHECK_FALSE(LoadShipBlueprint(path).has_value());
}

TEST_CASE("LoadShipBlueprint returns nullopt for a save from a newer schema version",
          "[save-file]") {
    const std::filesystem::path path = TempSavePath("sr_save_file_future_schema.sav");
    ShipBlueprint fromTheFuture = MakeBlueprint();
    fromTheFuture.schemaVersion = sr::kBlueprintSchemaVersion + 1;

    REQUIRE(SaveShipBlueprint(path, fromTheFuture));
    CHECK_FALSE(LoadShipBlueprint(path).has_value());

    std::filesystem::remove(path);
}
