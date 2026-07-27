#pragma once

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// Set by input or AI to credit a faction's stock (production/income); consumed and cleared by
// FactionEconomySystem the same tick, the same idiom as Combat.h's FireIntent.
struct DepositRequest {
    FactionId faction;
    int amount = 0;
};

// Set by input or AI to attempt spending a faction's stock; consumed and cleared the same tick.
// The outcome lands on the SAME requesting entity as a SpendResult for whatever asked to read
// back.
struct SpendRequest {
    FactionId faction;
    int amount = 0;
};

// Written by FactionEconomySystem once a SpendRequest resolves. Overwrites any previous result
// on the same entity -- a result from three ticks ago is not meant to be read as current.
struct SpendResult {
    bool succeeded = false;
};

}  // namespace sr
