#pragma once

#include <entt/entity/entity.hpp>
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
//
// architecture.md 12.30.1 -- `PlayerLocation` below is the source of truth; this tag is meant to
// be *derived* from it (P4-01), never emplaced directly by gameplay code. Until that derivation
// lands, nothing emplaces this component at all -- readers see an empty view rather than a stale
// tag, which is the safer of the two failure modes.
struct PlayerControlled {};

// The shell the player currently occupies: their own cockpit while flying, or the facility
// hardpoint they've selected while docked (architecture.md 12.24 step 5 / 12.30.1). Emplaced on
// that entity itself -- `shell` names the entity it lives on, which is what lets a system find
// "the" PlayerLocation with a plain view and still know which entity it names without also
// carrying the entt::entity handle separately. `PlayerControlled` is derived from `shell`'s rig
// root (P4-01); this is the only thing gameplay code ever writes.
struct PlayerLocation {
    entt::entity shell = entt::null;
};

}  // namespace sr
