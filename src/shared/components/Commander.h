#pragma once

#include <cstdint>

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// The three standing orders features.md 4.1 names for an AI sub-commander. Fleet dispatch
// destinations are not modeled -- there is no galaxy-level movement/rally-point concept yet
// (architecture.md 12.6's NavigationMap, which would supply one, is itself unbuilt) -- so
// `Dispatch` currently means "engaged, no override," leaving whatever engagement behavior already
// exists (NpcAiSystem) to run unmodified.
enum class CommanderOrders : std::uint8_t { Dispatch, Retreat, Defend };

// An AI sub-commander (architecture.md 12.2, #82). The commanded vessel is the entity carrying
// this component, not a separate reference -- destroying the capital destroys the commander for
// free, no special-case code needed.
//
// `network` is the volatile-side pointer into the sub-commander's own KnowledgeNetwork (Law 3's
// blueprint/live-form split, applied the same way NetworkOwner.h already documents): the durable
// data lives in core/knowledge/KnowledgeNetwork.h, keyed by this id, and outlives this entity.
// Destroying the entity only drops the reference -- it does not destroy the network (features.md
// 2.5's networks belong to whoever holds the id, and nothing here calls KnowledgeStore::Destroy).
//
// Writers: CommanderSystem sets `orders`. Whatever creates a sub-commander (not yet built -- see
// the ❓ open recruitment note in architecture.md 12.2) sets `network` and `faction` once.
struct Commander {
    KnowledgeNetworkId network;
    CommanderOrders orders = CommanderOrders::Defend;
    FactionId faction;
};

}  // namespace sr
