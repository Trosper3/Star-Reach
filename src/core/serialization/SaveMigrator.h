#pragma once

#include <optional>

#include "core/knowledge/KnowledgeNetwork.h"
#include "shared/blueprints/ShipBlueprint.h"

// Save schema migration (architecture.md section 5, section 9's legacy migration table -- a
// REWRITE of legacy core/SaveManager.cpp, not a port: the legacy version carried no schema
// versioning at all, and operated on entity-handle state rather than blueprint form).
//
// A save (core/serialization/SaveFile.h) is a ShipBlueprint's own encoded bytes --
// schemaVersion already travels as one of its ordinary fields (shared/blueprints/ShipBlueprint.h),
// so there is no separate outer header to invent. Migrate() is what a loader runs the
// freshly-decoded blueprint through before trusting it.
namespace sr::core::serialization {

// Migrates `blueprint` forward to kBlueprintSchemaVersion, applying whichever per-version steps
// this function's own dispatch (see the .cpp) applies. Returns nullopt if blueprint.schemaVersion
// is newer than this build understands (a save from a later game version) -- there is nothing to
// migrate FORWARD from there, and guessing would silently corrupt a design. A version below 1 is
// likewise rejected: 1 has been the floor since ShipBlueprint existed.
//
// Today this is an identity pass-through for schemaVersion == kBlueprintSchemaVersion: no schema
// bump has happened yet, so there is nothing to migrate FROM. The dispatch exists so the next
// bump has one place to add a step, instead of every save-reading call site growing its own ad
// hoc version check.
std::optional<ShipBlueprint> Migrate(ShipBlueprint blueprint);

// Migrates `store` forward to kKnowledgeSchemaVersion. `schemaVersion` is passed in separately --
// unlike ShipBlueprint, KnowledgeStore carries no version field of its own (KnowledgeNetwork.h's
// header comment explains why); SaveFile.h's LoadKnowledgeStore reads the leading version field it
// wrote and hands it here. Same floor/ceiling rules as Migrate(ShipBlueprint): nullopt for a
// version newer than this build understands, and nullopt below 1.
std::optional<sr::core::knowledge::KnowledgeStore> MigrateKnowledgeStore(
    int schemaVersion, sr::core::knowledge::KnowledgeStore store);

}  // namespace sr::core::serialization
