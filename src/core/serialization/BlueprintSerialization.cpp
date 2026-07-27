#include "core/serialization/BlueprintSerialization.h"

#include <algorithm>
#include <cstdint>

namespace sr::core::serialization {
namespace {

template <typename Tag>
void PutId(ByteWriter& writer, const sr::StringId<Tag>& id) {
    writer.PutString(id.str());
}

template <typename Tag>
sr::StringId<Tag> GetId(ByteReader& reader) {
    return sr::StringId<Tag>(reader.GetString());
}

void PutVec2(ByteWriter& writer, const sr::Vec2& v) {
    writer.Put(v.x);
    writer.Put(v.y);
}

sr::Vec2 GetVec2(ByteReader& reader) {
    sr::Vec2 v;
    v.x = reader.Get<float>();
    v.y = reader.Get<float>();
    return v;
}

}  // namespace

void Encode(ByteWriter& writer, const MountBlueprint& mount) {
    PutId(writer, mount.id);
    PutId(writer, mount.shell);
    PutId(writer, mount.attachedTo);
    PutVec2(writer, mount.localOffset);
    writer.Put(mount.localRotation);
    writer.Put(mount.traverseRadians);

    const uint16_t moduleCount =
        static_cast<uint16_t>(std::min(mount.modules.size(), std::size_t{65535}));
    writer.Put(moduleCount);
    for (uint16_t i = 0; i < moduleCount; ++i) {
        PutId(writer, mount.modules[i]);
    }
}

bool Decode(ByteReader& reader, MountBlueprint& out) {
    out.id = GetId<sr::MountIdTag>(reader);
    out.shell = GetId<sr::ShellIdTag>(reader);
    out.attachedTo = GetId<sr::MountIdTag>(reader);
    out.localOffset = GetVec2(reader);
    out.localRotation = reader.Get<float>();
    out.traverseRadians = reader.Get<float>();

    const uint16_t moduleCount = reader.Get<uint16_t>();
    if (!reader.Ok()) {
        return false;
    }
    out.modules.clear();
    out.modules.reserve(moduleCount);
    for (uint16_t i = 0; i < moduleCount; ++i) {
        out.modules.push_back(GetId<sr::ModuleIdTag>(reader));
        if (!reader.Ok()) {
            return false;
        }
    }
    return reader.Ok();
}

void Encode(ByteWriter& writer, const RigBlueprint& rig) {
    const uint16_t mountCount =
        static_cast<uint16_t>(std::min(rig.mounts.size(), std::size_t{65535}));
    writer.Put(mountCount);
    for (uint16_t i = 0; i < mountCount; ++i) {
        Encode(writer, rig.mounts[i]);
    }
}

bool Decode(ByteReader& reader, RigBlueprint& out) {
    const uint16_t mountCount = reader.Get<uint16_t>();
    if (!reader.Ok()) {
        return false;
    }
    out.mounts.clear();
    out.mounts.reserve(mountCount);
    for (uint16_t i = 0; i < mountCount; ++i) {
        MountBlueprint mount;
        if (!Decode(reader, mount)) {
            return false;
        }
        out.mounts.push_back(std::move(mount));
    }
    return true;
}

void Encode(ByteWriter& writer, const ShipBlueprint& blueprint) {
    PutId(writer, blueprint.id);
    writer.Put(blueprint.schemaVersion);
    writer.PutString(blueprint.displayName);
    PutId(writer, blueprint.faction);
    writer.Put(static_cast<uint8_t>(blueprint.mobile ? 1 : 0));
    writer.Put(blueprint.structuralMassLimit);
    Encode(writer, blueprint.rig);
}

bool Decode(ByteReader& reader, ShipBlueprint& out) {
    out.id = GetId<sr::BlueprintIdTag>(reader);
    out.schemaVersion = reader.Get<int>();
    out.displayName = reader.GetString();
    out.faction = GetId<sr::FactionIdTag>(reader);
    out.mobile = reader.Get<uint8_t>() != 0;
    out.structuralMassLimit = reader.Get<float>();
    if (!reader.Ok()) {
        return false;
    }
    return Decode(reader, out.rig);
}

}  // namespace sr::core::serialization
