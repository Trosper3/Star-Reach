#pragma once

#include <entt/entity/entity.hpp>

#include "shared/blueprints/Ids.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

enum class ContractType : unsigned char { Bounty, Escort };

// A contract, in its active/live form. There is no separate "offer" representation yet -- that
// belongs to a future contract-board/UI system; accepting a contract is authoring one of these
// directly onto the holder via AcceptContractRequest.
struct Contract {
    ContractType type = ContractType::Bounty;
    int rewardCredits = 0;

    // Bounty: kill killsRequired rigs of targetFaction (anywhere) while this contract is active.
    FactionId targetFaction;
    int killsRequired = 0;
    int killsDone = 0;

    // Escort: keep escortTarget alive until timeRemaining reaches zero. Fails immediately if the
    // target is destroyed first; otherwise completes once timeRemaining runs out with the target
    // still alive.
    entt::entity escortTarget = entt::null;
    float timeRemaining = 0.0f;  // Escort only; Bounty has no timer.
};

// Set by input or AI; consumed the same tick, the same idiom as Combat.h's FireIntent. Dropped
// without effect if the holder already has an active Contract -- "one active contract at a
// time", ported from legacy StarReach2's locked decision.
struct AcceptContractRequest {
    Contract contract;
};

// Tag: this destroyed rig has already been credited toward any Bounty contract watching its
// faction. Without it, ContractSystem would recount the same corpse on every future tick --
// nothing in this codebase currently destroys a rig root once DamageSystem tags it Destroyed
// (unlike a hardpoint's per-tick PendingDamage, which DamageSystem itself clears), so the tag
// would otherwise linger and be "seen" as a fresh kill forever.
struct KillCredited {};

}  // namespace sr
