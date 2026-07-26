#include "modes/space/data/SystemWorld.h"

#include <utility>

namespace sr::space {

SystemWorld::SystemWorld(std::string systemId, std::string displayName)
    : systemId_(std::move(systemId)), displayName_(std::move(displayName)) {
    if (displayName_.empty()) {
        displayName_ = systemId_;
    }
}

NetworkId SystemWorld::Track(entt::entity entity) {
    if (entity == entt::null) {
        return kInvalidNetworkId;
    }
    const auto key = static_cast<std::uint32_t>(entt::to_integral(entity));
    if (const auto existing = byEntity_.find(key); existing != byEntity_.end()) {
        return existing->second;
    }

    const NetworkId id = nextNetworkId_++;
    byNetworkId_.emplace(id, entity);
    byEntity_.emplace(key, id);
    return id;
}

void SystemWorld::Forget(NetworkId id) {
    const auto it = byNetworkId_.find(id);
    if (it == byNetworkId_.end()) {
        return;
    }
    byEntity_.erase(static_cast<std::uint32_t>(entt::to_integral(it->second)));
    byNetworkId_.erase(it);
}

entt::entity SystemWorld::Resolve(NetworkId id) const {
    const auto it = byNetworkId_.find(id);
    if (it == byNetworkId_.end()) {
        return entt::null;
    }
    // The mapping can outlive the entity by a tick if a system destroyed it without calling
    // Forget. Checking validity here means a stale id reads as "gone" rather than as a
    // different object that happens to have reused the slot.
    return registry_.valid(it->second) ? it->second : entt::null;
}

NetworkId SystemWorld::IdOf(entt::entity entity) const {
    if (entity == entt::null) {
        return kInvalidNetworkId;
    }
    const auto it = byEntity_.find(static_cast<std::uint32_t>(entt::to_integral(entity)));
    return it == byEntity_.end() ? kInvalidNetworkId : it->second;
}

}  // namespace sr::space
