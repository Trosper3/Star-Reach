#include "modes/space/systems/ConstructionSystem.h"

#include <vector>

#include "modes/space/factories/RigFactory.h"
#include "modes/space/factories/StationFactory.h"
#include "modes/space/systems/PlayerRecordSystem.h"
#include "shared/components/Construction.h"
#include "shared/components/Loot.h"

namespace sr::space::construction_system {
namespace {

// The player's own FactionId, not the requester's FactionRef -- the requester is the docked rig
// root the request was set on, which is the station itself while docked at a foreign one
// (architecture.md 12.30.3, amending 12.30.1). Only the player ever raises these requests: no NPC
// faction economy path emplaces BuildStationRequest/PlaceShipRequest (that side runs entirely
// through core/economy/FactionEconomy.h's SpendFactionStock).
FactionId RequesterFaction(const entt::registry& registry) {
    return player_record_system::FactionOf(registry);
}

void ProcessStationRequests(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> consumed;

    for (auto [self, request] : registry.view<BuildStationRequest>().each()) {
        consumed.push_back(self);

        Wallet* wallet = registry.try_get<Wallet>(self);
        if (wallet == nullptr || wallet->credits < request.cost) {
            continue;
        }

        rig_factory::SpawnParams params;
        params.blueprint = request.blueprint;
        params.faction = RequesterFaction(registry);
        params.position = request.position;
        params.rotation = request.rotation;

        const auto result = station_factory::Spawn(ctx.world, ctx.content, params);
        if (!result.ok()) {
            continue;
        }
        wallet->credits -= request.cost;
    }

    for (const entt::entity self : consumed) {
        registry.remove<BuildStationRequest>(self);
    }
}

void ProcessShipRequests(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> consumed;

    for (auto [self, request] : registry.view<PlaceShipRequest>().each()) {
        consumed.push_back(self);

        Wallet* wallet = registry.try_get<Wallet>(self);
        if (wallet == nullptr || wallet->credits < request.cost) {
            continue;
        }

        rig_factory::SpawnParams params;
        params.blueprint = request.blueprint;
        params.faction = RequesterFaction(registry);
        params.position = request.position;
        params.rotation = request.rotation;

        const auto result = rig_factory::Spawn(ctx.world, ctx.content, params);
        if (!result.ok()) {
            continue;
        }
        wallet->credits -= request.cost;
    }

    for (const entt::entity self : consumed) {
        registry.remove<PlaceShipRequest>(self);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    ProcessStationRequests(ctx);
    ProcessShipRequests(ctx);
}

}  // namespace sr::space::construction_system
