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

}  // namespace sr::core::diplomacy
