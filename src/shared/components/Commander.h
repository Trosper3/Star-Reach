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

// An AI sub-commander (architecture.md 12.2, #82). Lives on the **Bridge or cockpit hardpoint**
// it commands from (architecture.md 12.16 item 22), not the rig root -- a commander is a `Crew`
// module in a bridge shell (features.md 2.7), and a shell is a hardpoint entity. Find the vessel
// via that hardpoint's `ParentRig`. This buys a sharper rule than "destroy the capital, lose the
// commander": **destroy the bridge, lose the commander, keep the ship** -- decapitation is
// tactically distinct from destruction, the same shape the uncrewed hull (Rig.h's `Uncrewed`)
// already has for the crew shell in general. A hardpoint tagged `Destroyed` still carries this
// component (DamageSystem only tags, never strips -- Rig.h), so a reader must check for that tag
// itself; `CommanderSystem::HasLeadership` is the canonical example.
//
// `network` is the volatile-side pointer into the sub-commander's own KnowledgeNetwork (Law 3's
// blueprint/live-form split, applied the same way NetworkOwner.h already documents): the durable
// data lives in core/knowledge/KnowledgeNetwork.h, keyed by this id, and outlives this entity.
// Destroying the entity only drops the reference -- it does not destroy the network (features.md
// 2.5's networks belong to whoever holds the id, and nothing here calls KnowledgeStore::Destroy).
//
// Writers: CommanderSystem sets `orders`. `WorldGen::SeedCommanders` promotes an already-mounted
// officer (features.md 4.5 -- "not a separate acquisition track") to Commander at seed time;
// CaptureSystem reassigns `faction` when the hull it rides changes hands.
struct Commander {
    KnowledgeNetworkId network;
    CommanderOrders orders = CommanderOrders::Defend;
    FactionId faction;
};

}  // namespace sr
