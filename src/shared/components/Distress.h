#pragma once

#include <entt/entity/entity.hpp>

#include "shared/blueprints/Ids.h"
#include "shared/math/Vec2.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

enum class DistressType : unsigned char { Stranded, ShipUnderAttack };

// A distress call, in live form -- its own entity, not a hardpoint or a rig. One active call per
// registry at a time (ported from legacy StarReach2's "single active item" precedent, the same
// one ContractSystem's "one active contract at a time" already follows).
//
// Resolution differs by type, but both share the same shape: survive `windowSeconds` to succeed.
//   - ShipUnderAttack: succeeds if npcTarget is still alive when the window elapses; fails the
//     instant npcTarget is destroyed, regardless of the timer.
//   - Stranded: succeeds if acknowledged before the window elapses (a responder found them in
//     time); fails if the window elapses unacknowledged.
struct DistressCall {
    DistressType type = DistressType::Stranded;
    Vec2 position;
    entt::entity npcTarget = entt::null;  // ShipUnderAttack only.
    FactionId issuerFaction;
    float timer = 0.0f;  // Counts up from 0.
    float windowSeconds = 0.0f;
    int rewardCredits = 0;
    bool acknowledged = false;
    entt::entity responder = entt::null;  // Who acknowledged it; entt::null until then.
};

// Set by input or AI to raise a new call; dropped without effect if a DistressCall is already
// active. There is no producer yet -- a future NpcAiSystem Flee transition (ShipUnderAttack) or
// a future failed-warp handler (Stranded) would be the first ones to push this.
struct DistressCallRequest {
    DistressCall call;
};

// Set by input or AI; consumed and cleared by DistressSystem the same tick, the same idiom as
// Combat.h's FireIntent.
struct AcknowledgeDistressRequest {};

}  // namespace sr
