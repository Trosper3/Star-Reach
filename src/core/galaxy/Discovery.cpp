#include "core/galaxy/Discovery.h"

namespace sr::core::galaxy {
namespace {

const std::unordered_set<std::string>& EmptySet() {
    static const std::unordered_set<std::string> kEmpty;
    return kEmpty;
}

}  // namespace

bool DiscoveryState::IsDiscovered(const FactionId& faction, const std::string& systemId) const {
    const auto it = discovered_.find(faction);
    if (it == discovered_.end()) {
        return false;
    }
    return it->second.contains(systemId);
}

void DiscoveryState::Discover(const FactionId& faction, const std::string& systemId) {
    discovered_[faction].insert(systemId);
}

const std::unordered_set<std::string>& DiscoveryState::Discovered(const FactionId& faction) const {
    const auto it = discovered_.find(faction);
    return it == discovered_.end() ? EmptySet() : it->second;
}

}  // namespace sr::core::galaxy
