#pragma once

#include <entt/entity/entity.hpp>
#include <vector>

#include "shared/math/Vec2.h"

namespace sr {

// On a party leader's rig root: the follower rig roots that formation-fly around it and share
// its retaliation state. A party has exactly one leader; a rig with neither PartyLeader nor
// PartyMember is not in a party at all.
//
// The vector here is the same exception Rig::children documents (architecture.md Law 4): this
// list *is* the party's membership, not a stand-in for something that should be its own entity.
struct PartyLeader {
    std::vector<entt::entity> members;
};

// On a follower rig root: the leader it keeps station on.
struct PartyMember {
    entt::entity leader = entt::null;

    // Desired offset from the leader, in the leader's local frame. PartySystem rotates this by
    // the leader's current heading each tick, so the formation keeps its shape as the leader
    // turns instead of trailing a fixed world-space point.
    Vec2 formationOffset;
};

}  // namespace sr
