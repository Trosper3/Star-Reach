#include "core/serialization/SaveMigrator.h"

namespace sr::core::serialization {

std::optional<ShipBlueprint> Migrate(ShipBlueprint blueprint) {
    if (blueprint.schemaVersion > kBlueprintSchemaVersion) {
        return std::nullopt;  // From a newer game version; refuse to guess.
    }
    if (blueprint.schemaVersion < 1) {
        return std::nullopt;  // 1 has been the floor since ShipBlueprint existed.
    }

    // Add the next step here when kBlueprintSchemaVersion bumps, e.g.:
    //
    //   if (blueprint.schemaVersion == 1) {
    //       blueprint = MigrateV1ToV2(std::move(blueprint));
    //   }
    //
    // Falls through unchanged below for the current version -- there is only one so far.

    blueprint.schemaVersion = kBlueprintSchemaVersion;
    return blueprint;
}

std::optional<sr::core::knowledge::KnowledgeStore> MigrateKnowledgeStore(
    int schemaVersion, sr::core::knowledge::KnowledgeStore store) {
    if (schemaVersion > sr::core::knowledge::kKnowledgeSchemaVersion) {
        return std::nullopt;  // From a newer game version; refuse to guess.
    }
    if (schemaVersion < 1) {
        return std::nullopt;  // 1 has been the floor since KnowledgeStore saves existed.
    }

    // Add the next step here when kKnowledgeSchemaVersion bumps, mirroring Migrate(ShipBlueprint)
    // above. Falls through unchanged below for the current version -- there is only one so far.

    return store;
}

}  // namespace sr::core::serialization
