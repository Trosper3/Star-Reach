#include "core/serialization/SaveFile.h"

#include <cstdint>
#include <fstream>
#include <vector>

#include "core/serialization/BlueprintSerialization.h"
#include "core/serialization/ByteStream.h"
#include "core/serialization/KnowledgeSerialization.h"
#include "core/serialization/SaveMigrator.h"

using sr::core::knowledge::KnowledgeStore;

namespace sr::core::serialization {
namespace {

// Shared by both SaveX/LoadX pairs below -- the on-disk shape is always "encode into a
// ByteWriter, write the bytes." Free function rather than a template so it stays out of the
// header; neither caller needs it inlined.
bool WriteBytes(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    return file.good();
}

std::optional<std::vector<uint8_t>> ReadBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::nullopt;
    }

    const std::streamsize size = file.tellg();
    if (size <= 0) {
        return std::nullopt;
    }
    file.seekg(0);

    std::vector<uint8_t> data(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        return std::nullopt;
    }
    return data;
}

}  // namespace

bool SaveShipBlueprint(const std::filesystem::path& path, const ShipBlueprint& blueprint) {
    ByteWriter writer;
    Encode(writer, blueprint);
    return WriteBytes(path, writer.Data());
}

std::optional<ShipBlueprint> LoadShipBlueprint(const std::filesystem::path& path) {
    const auto data = ReadBytes(path);
    if (!data.has_value()) {
        return std::nullopt;
    }

    ByteReader reader(*data);
    ShipBlueprint blueprint;
    if (!Decode(reader, blueprint)) {
        return std::nullopt;
    }

    return Migrate(std::move(blueprint));
}

bool SaveKnowledgeStore(const std::filesystem::path& path, const KnowledgeStore& store) {
    ByteWriter writer;
    writer.Put(sr::core::knowledge::kKnowledgeSchemaVersion);
    Encode(writer, store);
    return WriteBytes(path, writer.Data());
}

std::optional<KnowledgeStore> LoadKnowledgeStore(const std::filesystem::path& path) {
    const auto data = ReadBytes(path);
    if (!data.has_value()) {
        return std::nullopt;
    }

    ByteReader reader(*data);
    const int schemaVersion = reader.Get<int>();
    if (!reader.Ok()) {
        return std::nullopt;
    }

    KnowledgeStore store;
    if (!Decode(reader, store)) {
        return std::nullopt;
    }

    return MigrateKnowledgeStore(schemaVersion, std::move(store));
}

}  // namespace sr::core::serialization
