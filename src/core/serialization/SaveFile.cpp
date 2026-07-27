#include "core/serialization/SaveFile.h"

#include <cstdint>
#include <fstream>
#include <vector>

#include "core/serialization/BlueprintSerialization.h"
#include "core/serialization/ByteStream.h"
#include "core/serialization/SaveMigrator.h"

namespace sr::core::serialization {

bool SaveShipBlueprint(const std::filesystem::path& path, const ShipBlueprint& blueprint) {
    ByteWriter writer;
    Encode(writer, blueprint);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    const std::vector<uint8_t>& data = writer.Data();
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    return file.good();
}

std::optional<ShipBlueprint> LoadShipBlueprint(const std::filesystem::path& path) {
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

    ByteReader reader(data);
    ShipBlueprint blueprint;
    if (!Decode(reader, blueprint)) {
        return std::nullopt;
    }

    return Migrate(std::move(blueprint));
}

}  // namespace sr::core::serialization
