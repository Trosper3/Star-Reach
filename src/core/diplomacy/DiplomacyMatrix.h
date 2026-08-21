#pragma once

#include <map>
#include <utility>

#include "core/diplomacy/Relation.h"
#include "shared/blueprints/Ids.h"

namespace sr::core::diplomacy {

// The faction-vs-faction relation matrix (architecture.md Law 8) -- galaxy-wide state, so it
// lives here in core/ rather than in any one entt::registry: a party retaliation check or a raid
// dispatch decision needs the same answer regardless of which system's registry is asking.
//
// Mode-agnostic, registry-agnostic, and render-agnostic: sr_core cannot link raylib, which is
// what keeps this constructible and testable with no window, no registry, and no mode.
//
// Relations are symmetric -- Set(a, b, r) and Set(b, a, r) are the same call -- because this
// models mutual diplomatic stance, not a one-sided override layer. A faction is always Friendly
// with itself; Set() on a pair of equal ids is a no-op.
class DiplomacyMatrix {
public:
    Relation Get(const FactionId& a, const FactionId& b) const;
    void Set(const FactionId& a, const FactionId& b, Relation relation);

    // Nudges the pair exactly one band toward `target` (features.md 5.3's "slow drift" writers --
    // trade volume, blockade, embargo) -- never further, so a single event can't jump straight
    // from Neutral to Allied or Hostile. A no-op once the pair already reads `target`. Set()'s own
    // a == b no-op still applies underneath.
    void DriftToward(const FactionId& a, const FactionId& b, Relation target);

private:
    using Pair = std::pair<FactionId, FactionId>;
    static Pair Canonical(const FactionId& a, const FactionId& b);

    std::map<Pair, Relation> relations_;
};

}  // namespace sr::core::diplomacy
