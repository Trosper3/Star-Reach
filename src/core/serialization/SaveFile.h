#pragma once

#include <filesystem>
#include <optional>

#include "shared/blueprints/ShipBlueprint.h"

// The .sav half of the unified serialization pipeline (architecture.md section 5): the same
// ByteWriter/ByteReader and Encode/Decode (core/serialization/BlueprintSerialization.h) that
// will eventually pack a wire packet, written to and read from a file instead of a socket.
// Every load runs through SaveMigrator::Migrate before returning, so a caller never sees a save
// written at an older schema version -- or one from a newer one it cannot understand.
namespace sr::core::serialization {

// Overwrites `path` with `blueprint`'s current encoded form. Returns false if the file could not
// be opened for writing.
bool SaveShipBlueprint(const std::filesystem::path& path, const ShipBlueprint& blueprint);

// Returns nullopt if the file cannot be opened, its bytes fail to decode (a truncated or corrupt
// file), or Migrate() refuses its schemaVersion.
std::optional<ShipBlueprint> LoadShipBlueprint(const std::filesystem::path& path);

}  // namespace sr::core::serialization
