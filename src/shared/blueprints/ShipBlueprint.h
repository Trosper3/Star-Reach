#pragma once

#include <string>

#include "shared/blueprints/Ids.h"
#include "shared/blueprints/RigBlueprint.h"

namespace sr {

// Bumped whenever the on-disk shape of a blueprint changes. Saves and Templates carry the
// version they were written with, and SaveMigrator converts older layouts forward
// (architecture.md section 5). A blueprint with no version is rejected, not assumed current --
// features.md validation rule 9.
inline constexpr int kBlueprintSchemaVersion = 1;

// The complete authored form of a craft or station (Law 3, blueprint form).
//
// This is what a player Template *is*, what a save file stores, what gets sold to a faction, and
// what a faction manufactures from. It contains no entity handles and no pointers, which is what
// makes all four of those uses the same code path.
struct ShipBlueprint {
    BlueprintId id;
    int schemaVersion = kBlueprintSchemaVersion;
    std::string displayName;

    // Empty for a player Template. Set for authored faction content.
    FactionId faction;

    // Which factory a blueprint is meant for (StationFactory::Spawn rejects mobile: true) and
    // whether Validation requires at least one engine shell (validation rule 5). Does NOT decide
    // whether RigFactory gives the rig Propulsion/PhysicsSystem handling -- every rig gets both,
    // and a rig with no living engine hardpoints simply aggregates to zero thrust and does not
    // move (architecture.md 12.25: capability is emergent, not authored). One flag, checked in
    // one place, instead of a parallel StationBlueprint type -- see Law 4 on what parallel types
    // cost.
    bool mobile = true;

    // Total rig mass may not exceed this (validation rule 4). It is the ceiling half of the
    // constraints puzzle; the floor is that every module still needs power.
    float structuralMassLimit = 0.0f;

    RigBlueprint rig;
};

}  // namespace sr
