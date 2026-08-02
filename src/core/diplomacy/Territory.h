#pragma once

#include <string>
#include <unordered_map>

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

private:
    std::unordered_map<std::string, FactionId> owners_;
};

}  // namespace sr::core::diplomacy
