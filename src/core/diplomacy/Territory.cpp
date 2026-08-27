#include "core/diplomacy/Territory.h"

namespace sr::core::diplomacy {

FactionId Territory::Owner(const std::string& systemId) const {
    const auto it = owners_.find(systemId);
    return it != owners_.end() ? it->second : FactionId();
}

void Territory::Claim(const std::string& systemId, const FactionId& faction) {
    owners_[systemId] = faction;
}

void Territory::Release(const std::string& systemId) {
    owners_.erase(systemId);
}

void Territory::ReleaseAll(const FactionId& faction) {
    for (auto it = owners_.begin(); it != owners_.end();) {
        if (it->second == faction) {
            it = owners_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<std::pair<std::string, FactionId>> Territory::ClaimedSystems() const {
    std::vector<std::pair<std::string, FactionId>> claims;
    claims.reserve(owners_.size());
    for (const auto& [systemId, owner] : owners_) {
        claims.emplace_back(systemId, owner);
    }
    return claims;
}

}  // namespace sr::core::diplomacy
