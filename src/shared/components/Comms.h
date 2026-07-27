#pragma once

#include <entt/entity/entity.hpp>
#include <string>
#include <vector>

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

struct CommsEntry {
    std::string text;
    bool fromPlayer = false;
};

// Tag: the one entity per registry carrying the CommsLog -- System.h's "one legitimate cache"
// exception (a singleton entity rather than mode-held state, Law 6). CommsSystem creates it
// lazily the first time anything is logged.
struct CommsLogSingleton {};

struct CommsLog {
    std::vector<CommsEntry> entries;
};

// Set by input or AI; consumed and cleared by CommsSystem the same tick, the same idiom as
// Combat.h's FireIntent.
struct HailRequest {
    entt::entity target = entt::null;
};

}  // namespace sr
