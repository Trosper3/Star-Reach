#pragma once

#include <filesystem>
#include <optional>

#include "core/knowledge/KnowledgeNetwork.h"
#include "shared/blueprints/ShipBlueprint.h"

// The .sav half of the unified serialization pipeline (architecture.md section 5): the same
// ByteWriter/ByteReader and Encode/Decode (core/serialization/BlueprintSerialization.h,
// core/serialization/KnowledgeSerialization.h) that will eventually pack a wire packet, written
// to and read from a file instead of a socket. Every load runs through SaveMigrator before
// returning, so a caller never sees a save written at an older schema version -- or one from a
// newer one it cannot understand.
namespace sr::core::serialization {

// Overwrites `path` with `blueprint`'s current encoded form. Returns false if the file could not
// be opened for writing.
bool SaveShipBlueprint(const std::filesystem::path& path, const ShipBlueprint& blueprint);

// Returns nullopt if the file cannot be opened, its bytes fail to decode (a truncated or corrupt
// file), or Migrate() refuses its schemaVersion.
std::optional<ShipBlueprint> LoadShipBlueprint(const std::filesystem::path& path);

// The networks section architecture.md 12.1 adds to this file. Overwrites `path` with `store`'s
// current encoded form, framed with kKnowledgeSchemaVersion (KnowledgeNetwork.h's header comment
// explains why the version is framing here rather than a struct field). Returns false if the file
// could not be opened for writing.
bool SaveKnowledgeStore(const std::filesystem::path& path,
                        const sr::core::knowledge::KnowledgeStore& store);

// Returns nullopt if the file cannot be opened, its bytes fail to decode, or the leading schema
// version is one MigrateKnowledgeStore refuses.
std::optional<sr::core::knowledge::KnowledgeStore> LoadKnowledgeStore(
    const std::filesystem::path& path);

}  // namespace sr::core::serialization
