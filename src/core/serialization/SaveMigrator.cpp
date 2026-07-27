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

}  // namespace sr::core::serialization
