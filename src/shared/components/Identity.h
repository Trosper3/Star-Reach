#pragma once

#include <string>

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// The blueprint this entity was instantiated from. The bridge back to authored data -- a save
// writes this id, never the entt::entity (Law 2, hard rule 1).
struct BlueprintRef {
    BlueprintId id;
};

// Which mount of that blueprint this hardpoint entity is. Lets a save round-trip per-hardpoint
// damage without serializing handles.
struct MountRef {
    MountId id;
};

// Faction ownership. The key resolves against features.md section 5.2; the relation matrix
// itself lives in core/diplomacy/, not in any registry (Law 2, hard rule 2).
struct FactionRef {
    FactionId id;
};

struct DisplayName {
    std::string value;
};

// Tag: this entity is the player's current vessel. Exactly one per registry, and it moves rather
// than duplicating when the player transitions to a capital's Bridge (features.md section 4).
struct PlayerControlled {};

}  // namespace sr
