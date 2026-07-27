#pragma once

#include "core/serialization/ByteStream.h"
#include "shared/blueprints/ShipBlueprint.h"

// The unified rig wire/save format (architecture.md Law 4, Law 3).
//
// One MountBlueprint shape serializes every hardpoint-bearing rig -- fighter, capital, or player
// station -- because RigBlueprint already IS the structure RigBlueprint.h's own comment names as
// the fix for legacy's four separate hardpoint wire formats (CapitalHardpointSnapshot,
// PlayerStationHardpointSnapshot, FighterHardpointSnapshot, FighterMountReport). This file is
// what makes that structural unification a serialization unification too: one Encode/Decode
// pair, not four.
//
// Operates on blueprint form only (Law 3): nothing here takes or returns an entt::entity.
// Converting a live rig to/from its blueprint snapshot is a factory-adjacent concern that lives
// wherever that conversion is first needed, not in this file.
namespace sr::core::serialization {

void Encode(ByteWriter& writer, const MountBlueprint& mount);
bool Decode(ByteReader& reader, MountBlueprint& out);

void Encode(ByteWriter& writer, const RigBlueprint& rig);
bool Decode(ByteReader& reader, RigBlueprint& out);

// The top-level unit both a .sav file and a wire packet carry. schemaVersion travels as an
// ordinary ShipBlueprint field (see ShipBlueprint.h) rather than as framing this function adds,
// so a caller building an actual file or packet wraps this with whatever header it needs;
// Encode/Decode's job stops at the blueprint's own bytes.
void Encode(ByteWriter& writer, const ShipBlueprint& blueprint);
bool Decode(ByteReader& reader, ShipBlueprint& out);

}  // namespace sr::core::serialization
