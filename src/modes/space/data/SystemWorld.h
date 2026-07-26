#pragma once

#include <cstdint>
#include <entt/entity/registry.hpp>
#include <string>
#include <unordered_map>

namespace sr::space {

// A stable handle that may cross a save file, the wire, or a warp (Law 2, hard rule 1).
//
// entt::entity is registry-local and is invalidated by travel, so nothing persistent may hold
// one. Anything that needs to name an object outside this registry names it by NetworkId, and
// SystemWorld is the only translator.
using NetworkId = std::uint64_t;
inline constexpr NetworkId kInvalidNetworkId = 0;

// One star system: its registry, and the id mapping that lets the outside world refer into it.
//
// Law 2 -- there is no global registry. Ticking the active system at 60 Hz and its neighbors at
// 5 Hz is then a question of which SystemWorlds get ticked, not of filtering every query by a
// SystemId column. Warp is a handoff: serialize out of A, instantiate into B, tear down A.
class SystemWorld {
public:
    explicit SystemWorld(std::string systemId, std::string displayName = {});

    // Non-copyable: a registry is heavyweight and duplicating one would silently break the
    // "exactly one registry per system" invariant that everything above depends on.
    SystemWorld(const SystemWorld&) = delete;
    SystemWorld& operator=(const SystemWorld&) = delete;
    SystemWorld(SystemWorld&&) = default;

    entt::registry& Registry() { return registry_; }
    const entt::registry& Registry() const { return registry_; }

    const std::string& SystemId() const { return systemId_; }
    const std::string& DisplayName() const { return displayName_; }

    // Assigns and records a stable id for an entity. Called by factories on rig roots -- not on
    // every hardpoint. A hardpoint is addressed as (rig NetworkId, MountId), which is exactly
    // the pair that survives a rig being rebuilt on another machine.
    NetworkId Track(entt::entity entity);

    // Releases the mapping. Must be called when a tracked entity is destroyed, or Resolve()
    // will hand out a recycled handle belonging to a different object.
    void Forget(NetworkId id);

    // Returns entt::null for an unknown or stale id. Callers must check -- an intent from a
    // remote machine can name something that died locally two ticks ago.
    entt::entity Resolve(NetworkId id) const;
    NetworkId IdOf(entt::entity entity) const;

private:
    std::string systemId_;
    std::string displayName_;
    entt::registry registry_;

    std::unordered_map<NetworkId, entt::entity> byNetworkId_;
    std::unordered_map<std::uint32_t, NetworkId> byEntity_;
    NetworkId nextNetworkId_ = 1;
};

}  // namespace sr::space
