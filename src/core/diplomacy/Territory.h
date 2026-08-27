#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "shared/blueprints/Ids.h"

namespace sr::core::diplomacy {

// System-level territory ownership, galaxy-wide state per Law 8. Keyed by the plain system id
// string (matching SystemWorld::SystemId()'s type -- core/ cannot depend on modes/space/, so this
// deliberately does not introduce a new StringId tag for a concept modes/space/ already names
// with a bare std::string).
//
// An unclaimed system's owner is the empty FactionId, not a sentinel or an optional -- Law 3
// treats "no id" as already meaningful for a stable-string identity.
class Territory {
public:
    FactionId Owner(const std::string& systemId) const;
    void Claim(const std::string& systemId, const FactionId& faction);
    void Release(const std::string& systemId);

    // Releases every system `faction` currently owns -- features.md 5.1's "territory becomes
    // unclaimed" on collapse (architecture.md 12.3). A linear scan rather than a reverse index:
    // Territory is queried far more often by systemId than walked by faction, and collapse is a
    // rare, one-shot event, not a per-tick operation.
    void ReleaseAll(const FactionId& faction);

    // Every currently-claimed system and its owner -- for a reader building a galaxy-wide picture
    // rather than looking up one id at a time. `NavigationMap`'s territory aggregation
    // (architecture.md 12.35) is this class's first such reader. Unclaimed systems never appear:
    // Territory only records claims, and there is no roster of "every system that exists" for it
    // to draw on (that gap is documented at modes/space/ui/NavigationMap.h). Unordered -- callers
    // that need a stable order already sort their own output.
    std::vector<std::pair<std::string, FactionId>> ClaimedSystems() const;

private:
    std::unordered_map<std::string, FactionId> owners_;
};

}  // namespace sr::core::diplomacy
