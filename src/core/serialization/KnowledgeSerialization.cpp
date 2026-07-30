#include "core/serialization/KnowledgeSerialization.h"

#include <algorithm>
#include <cstdint>

using sr::KnowledgeNetworkId;
using sr::core::knowledge::KnowledgeNetwork;
using sr::core::knowledge::KnowledgeStore;
using sr::core::knowledge::NetworkOwnerKind;

namespace sr::core::serialization {
namespace {

void PutStringSet(ByteWriter& writer, const std::unordered_set<std::string>& values) {
    const uint16_t count = static_cast<uint16_t>(std::min(values.size(), std::size_t{65535}));
    writer.Put(count);
    uint16_t written = 0;
    for (const std::string& value : values) {
        if (written >= count) {
            break;
        }
        writer.PutString(value);
        ++written;
    }
}

bool GetStringSet(ByteReader& reader, std::unordered_set<std::string>& out) {
    const uint16_t count = reader.Get<uint16_t>();
    if (!reader.Ok()) {
        return false;
    }
    out.clear();
    for (uint16_t i = 0; i < count; ++i) {
        out.insert(reader.GetString());
        if (!reader.Ok()) {
            return false;
        }
    }
    return true;
}

}  // namespace

void Encode(ByteWriter& writer, const KnowledgeNetwork& network) {
    writer.Put(static_cast<uint8_t>(network.ownerKind));
    PutStringSet(writer, network.unlockedBlueprints);
    PutStringSet(writer, network.savedTemplates);
    PutStringSet(writer, network.discoveredSystems);
}

bool Decode(ByteReader& reader, KnowledgeNetwork& out) {
    out.ownerKind = static_cast<NetworkOwnerKind>(reader.Get<uint8_t>());
    if (!reader.Ok()) {
        return false;
    }
    if (!GetStringSet(reader, out.unlockedBlueprints)) {
        return false;
    }
    if (!GetStringSet(reader, out.savedTemplates)) {
        return false;
    }
    return GetStringSet(reader, out.discoveredSystems);
}

void Encode(ByteWriter& writer, const KnowledgeStore& store) {
    writer.Put(store.NextIdCounter());
    const auto& all = store.All();
    const uint32_t count = static_cast<uint32_t>(all.size());
    writer.Put(count);
    for (const auto& [id, network] : all) {
        writer.PutString(id.str());
        Encode(writer, network);
    }
}

bool Decode(ByteReader& reader, KnowledgeStore& out) {
    // Preserves the id counter across the round trip -- without it, Create() after a load would
    // hand out ids already used by the networks just decoded below.
    const uint64_t nextId = reader.Get<uint64_t>();
    if (!reader.Ok()) {
        return false;
    }
    const uint32_t count = reader.Get<uint32_t>();
    if (!reader.Ok()) {
        return false;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const std::string idStr = reader.GetString();
        if (!reader.Ok()) {
            return false;
        }
        KnowledgeNetwork network;
        if (!Decode(reader, network)) {
            return false;
        }
        out.LoadNetwork(KnowledgeNetworkId(idStr), std::move(network));
    }
    out.SetNextIdCounter(nextId);
    return true;
}

}  // namespace sr::core::serialization
