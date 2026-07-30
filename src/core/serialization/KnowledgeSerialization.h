#pragma once

#include "core/knowledge/KnowledgeNetwork.h"
#include "core/serialization/ByteStream.h"

// Save/wire format for the knowledge network store (architecture.md 12.1's "SaveFile gains a
// networks section").
//
// Unlike ShipBlueprint (BlueprintSerialization.h), KnowledgeStore carries no per-instance
// schemaVersion field of its own to travel as ordinary content -- there is no single authored
// object being versioned, only a map of networks. So schemaVersion is SaveFile.h's job: it writes
// kKnowledgeSchemaVersion as one leading field around whatever Encode()/Decode() below produce,
// the same way a wire packet would need an outer header this format otherwise has no reason to
// carry. Encode()/Decode() here stay version-agnostic on purpose.
namespace sr::core::serialization {

void Encode(ByteWriter& writer, const sr::core::knowledge::KnowledgeNetwork& network);
bool Decode(ByteReader& reader, sr::core::knowledge::KnowledgeNetwork& out);

void Encode(ByteWriter& writer, const sr::core::knowledge::KnowledgeStore& store);
bool Decode(ByteReader& reader, sr::core::knowledge::KnowledgeStore& out);

}  // namespace sr::core::serialization
