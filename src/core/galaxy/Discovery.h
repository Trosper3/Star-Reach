#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "shared/blueprints/Ids.h"

namespace sr::core::galaxy {

// Per-faction discovered-system knowledge -- galaxy-wide state (Law 8), so it lives here rather
// than in any one entt::registry: whether a system has been discovered is a fact about the
// galaxy, not about whichever SystemWorld happens to be ticking.
//
// Mode-agnostic, registry-agnostic, and render-agnostic -- sr_core cannot link raylib, which is
// what keeps this constructible and testable with no window, no registry, and no mode.
//
// Discovery is per faction rather than per player: legacy StarReach2 pooled it the same way
// (DiscoverySync/SystemDiscovered in net/Protocol.h -- deferred with net/, but the state model
// does not need multiplayer to exist). A faction's members share what any one of them has
// found, the "shared knowledge" architecture.md section 4's DiscoverySystem row names.
class DiscoveryState {
public:
    bool IsDiscovered(const FactionId& faction, const std::string& systemId) const;

    // No-op if the faction has already discovered this system.
    void Discover(const FactionId& faction, const std::string& systemId);

    const std::unordered_set<std::string>& Discovered(const FactionId& faction) const;

private:
    std::unordered_map<FactionId, std::unordered_set<std::string>> discovered_;
};

}  // namespace sr::core::galaxy
