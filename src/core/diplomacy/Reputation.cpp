#include "core/diplomacy/Reputation.h"

#include <algorithm>
#include <cmath>

namespace sr::core::diplomacy {

float Reputation::Score(const FactionId& faction) const {
    const auto it = score_.find(faction);
    return it != score_.end() ? it->second : 0.0f;
}

void Reputation::Adjust(const FactionId& faction, float delta) {
    if (delta == 0.0f) {
        return;
    }
    score_[faction] += delta;
}

Relation Reputation::ThresholdRelation(const FactionId& faction) const {
    const float score = Score(faction);
    if (score >= kFriendlyThreshold) {
        return Relation::Friendly;
    }
    if (score <= kHostileThreshold) {
        return Relation::Hostile;
    }
    return Relation::Neutral;
}

void Reputation::Tick(float dt) {
    if (dt <= 0.0f) {
        return;
    }
    const float step = kDecayPerSecond * dt;
    for (auto& [faction, score] : score_) {
        if (std::abs(score) <= step) {
            score = 0.0f;
        } else {
            score -= std::copysign(step, score);
        }
    }
}

}  // namespace sr::core::diplomacy
