#include "modes/space/systems/PlayerRecordSystem.h"

#include "shared/components/Identity.h"

namespace sr::space::player_record_system {
namespace {

// System.h's "one legitimate cache" exception: a singleton entity rather than mode-held state
// (Law 6). Mirrors CommsSystem.cpp's EnsureLog exactly.
entt::entity EnsureRecord(entt::registry& registry) {
    for (auto [entity] : registry.view<PlayerRecordSingleton>().each()) {
        return entity;
    }
    const entt::entity record = registry.create();
    registry.emplace<PlayerRecordSingleton>(record);
    registry.emplace<PlayerFaction>(record);
    return record;
}

}  // namespace

void SetFaction(entt::registry& registry, const FactionId& faction) {
    const entt::entity record = EnsureRecord(registry);
    registry.get<PlayerFaction>(record).id = faction;
}

FactionId FactionOf(const entt::registry& registry) {
    for (auto [entity, faction] : registry.view<PlayerFaction>().each()) {
        (void)entity;
        return faction.id;
    }
    return FactionId{};
}

}  // namespace sr::space::player_record_system
