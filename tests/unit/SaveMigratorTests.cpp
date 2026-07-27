#include <catch2/catch_test_macros.hpp>

#include "core/serialization/SaveMigrator.h"

using sr::ShipBlueprint;
using sr::core::serialization::Migrate;

namespace {

ShipBlueprint MakeBlueprint(int schemaVersion) {
    ShipBlueprint bp;
    bp.id = sr::BlueprintId("aegis_vanguard");
    bp.schemaVersion = schemaVersion;
    bp.displayName = "Aegis Vanguard";
    return bp;
}

}  // namespace

TEST_CASE("Migrate passes the current schema version through unchanged", "[save-migrator]") {
    const ShipBlueprint original = MakeBlueprint(sr::kBlueprintSchemaVersion);
    const auto migrated = Migrate(original);

    REQUIRE(migrated.has_value());
    CHECK(migrated->schemaVersion == sr::kBlueprintSchemaVersion);
    CHECK(migrated->id.str() == original.id.str());
    CHECK(migrated->displayName == original.displayName);
}

TEST_CASE("Migrate refuses a schema version newer than this build understands", "[save-migrator]") {
    const ShipBlueprint fromTheFuture = MakeBlueprint(sr::kBlueprintSchemaVersion + 1);
    CHECK_FALSE(Migrate(fromTheFuture).has_value());
}

TEST_CASE("Migrate refuses a schema version below the historical floor of 1", "[save-migrator]") {
    CHECK_FALSE(Migrate(MakeBlueprint(0)).has_value());
    CHECK_FALSE(Migrate(MakeBlueprint(-1)).has_value());
}
