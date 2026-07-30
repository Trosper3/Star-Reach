#pragma once

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// The volatile half of Law 3's blueprint/live-form split, applied to knowledge networks: the
// durable KnowledgeNetwork data (unlocked blueprints, saved Templates, discovered systems) lives
// in core/knowledge/KnowledgeNetwork.h, keyed by KnowledgeNetworkId. This component is only the
// entity-side pointer into that store -- never the data itself (Law 2's "entity handles never
// cross a system boundary" cuts the other way here: the id crosses freely, the store does not
// live in a registry at all).
//
// Writers: CommanderSystem sets this on a sub-commander entity when it is created (architecture.md
// 12.2, #82). The player's own network id is bootstrapped once per save, outside any one system --
// not yet built as of this component landing (#78).
struct NetworkOwner {
    KnowledgeNetworkId network;
};

}  // namespace sr
