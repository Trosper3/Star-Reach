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
    // features.md 5.3's six-band range, architecture.md 12.32: Friendly keeps its old ±25
    // boundary in spirit (renamed here to match the widened band names), and Allied/Distrustful/
    // Hostile/War slot in at the band edges §5.3's table itself specifies.
    static constexpr float kAlliedThreshold = 50.0f;
    static constexpr float kFriendlyThreshold = 15.0f;
    static constexpr float kDistrustfulThreshold = -15.0f;
    static constexpr float kHostileThreshold = -50.0f;
    static constexpr float kWarThreshold = -85.0f;

    // Unset factions start at 0 (Neutral).
    float Score(const FactionId& faction) const;

    void Adjust(const FactionId& faction, float delta);

    // Score() mapped through the band thresholds above.
    Relation ThresholdRelation(const FactionId& faction) const;

    // Pulls every tracked faction's score back toward 0 at kDecayPerSecond, without overshooting.
    void Tick(float dt);

private:
    static constexpr float kDecayPerSecond = 1.0f;

    std::unordered_map<FactionId, float> score_;
};

}  // namespace sr::core::diplomacy
