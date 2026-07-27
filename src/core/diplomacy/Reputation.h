#pragma once

#include <unordered_map>

#include "core/diplomacy/Relation.h"
#include "shared/blueprints/Ids.h"

namespace sr::core::diplomacy {

// A tracked actor's continuous standing with each faction (-100..100), galaxy-wide state per Law
// 8. This does not replace DiplomacyMatrix's faction-vs-faction relations -- it answers "how does
// this faction feel about the actor we're tracking right now," which drifts continuously via
// gameplay events (kills, trade, contracts) instead of sitting on a fixed matrix entry.
//
// Deliberately actor-agnostic rather than hardcoding a single player faction (architecture.md's
// faction model has no playerOwned flag): callers key events by whichever FactionId they are
// tracking standing for.
class Reputation {
public:
    static constexpr float kFriendlyThreshold = 25.0f;
    static constexpr float kHostileThreshold = -25.0f;

    // Unset factions start at 0 (Neutral).
    float Score(const FactionId& faction) const;

    void Adjust(const FactionId& faction, float delta);

    // Score() mapped through the friendly/hostile thresholds above.
    Relation ThresholdRelation(const FactionId& faction) const;

    // Pulls every tracked faction's score back toward 0 at kDecayPerSecond, without overshooting.
    void Tick(float dt);

private:
    static constexpr float kDecayPerSecond = 1.0f;

    std::unordered_map<FactionId, float> score_;
};

}  // namespace sr::core::diplomacy
