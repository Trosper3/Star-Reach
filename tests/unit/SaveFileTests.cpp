#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "core/serialization/KnowledgeSerialization.h"
#include "core/serialization/SaveFile.h"

using sr::ShipBlueprint;
using sr::core::knowledge::KnowledgeStore;
using sr::core::knowledge::NetworkEntryKind;
using sr::core::knowledge::NetworkOwnerKind;
using sr::core::serialization::LoadKnowledgeStore;
using sr::core::serialization::LoadShipBlueprint;
using sr::core::serialization::SaveKnowledgeStore;
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

TEST_CASE("SaveKnowledgeStore then LoadKnowledgeStore round-trips every network", "[save-file]") {
    const std::filesystem::path path = TempSavePath("sr_save_knowledge_roundtrip.sav");

    KnowledgeStore original;
    const auto player = original.Create(NetworkOwnerKind::Player);
    original.Grant(player, NetworkEntryKind::UnlockedBlueprint, "pulse_cannon_i");
    original.Grant(player, NetworkEntryKind::SavedTemplate, "aegis_vanguard");
    original.Grant(player, NetworkEntryKind::DiscoveredSystem, "sol");
    const auto faction = original.Create(NetworkOwnerKind::Faction);
    original.Grant(faction, NetworkEntryKind::SavedTemplate, "reaver_raider");

    REQUIRE(SaveKnowledgeStore(path, original));
    const auto loaded = LoadKnowledgeStore(path);
    REQUIRE(loaded.has_value());

    const auto* loadedPlayer = loaded->Get(player);
    REQUIRE(loadedPlayer != nullptr);
    CHECK(loadedPlayer->unlockedBlueprints.contains("pulse_cannon_i"));
    CHECK(loadedPlayer->savedTemplates.contains("aegis_vanguard"));
    CHECK(loadedPlayer->discoveredSystems.contains("sol"));
    REQUIRE(loaded->Get(faction) != nullptr);
    CHECK(loaded->Get(faction)->savedTemplates.contains("reaver_raider"));

    // save -> load -> save again must preserve the same contents (issue #78's explicit test).
    const std::filesystem::path resavedPath = TempSavePath("sr_save_knowledge_resaved.sav");
    REQUIRE(SaveKnowledgeStore(resavedPath, *loaded));
    const auto reloaded = LoadKnowledgeStore(resavedPath);
    REQUIRE(reloaded.has_value());
    REQUIRE(reloaded->Get(player) != nullptr);
    CHECK(reloaded->Get(player)->unlockedBlueprints == loadedPlayer->unlockedBlueprints);
    CHECK(reloaded->Get(player)->savedTemplates == loadedPlayer->savedTemplates);
    CHECK(reloaded->Get(player)->discoveredSystems == loadedPlayer->discoveredSystems);

    std::filesystem::remove(path);
    std::filesystem::remove(resavedPath);
}

TEST_CASE("LoadKnowledgeStore returns nullopt for a nonexistent file", "[save-file]") {
    const std::filesystem::path path = TempSavePath("sr_save_knowledge_does_not_exist.sav");
    std::filesystem::remove(path);
    CHECK_FALSE(LoadKnowledgeStore(path).has_value());
}

TEST_CASE("LoadKnowledgeStore returns nullopt for a save from a newer schema version",
          "[save-file]") {
    const std::filesystem::path path = TempSavePath("sr_save_knowledge_future_schema.sav");

    // SaveKnowledgeStore always writes the current schema version, so this writes the bytes
    // directly to simulate a save from a future build -- the same shape SaveFileTests uses for
    // ShipBlueprint, adapted since KnowledgeStore has no schemaVersion field of its own to bump.
    sr::core::serialization::ByteWriter writer;
    writer.Put(sr::core::knowledge::kKnowledgeSchemaVersion + 1);
    KnowledgeStore empty;
    sr::core::serialization::Encode(writer, empty);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    const auto& data = writer.Data();
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    file.close();

    CHECK_FALSE(LoadKnowledgeStore(path).has_value());

    std::filesystem::remove(path);
}
