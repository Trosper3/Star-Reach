#include "modes/space/systems/ConstructionSystem.h"

#include <vector>

#include "core/events/Intent.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/factories/RigFactory.h"
#include "modes/space/factories/StationFactory.h"
#include "modes/space/systems/PlayerRecordSystem.h"
#include "shared/blueprints/Validation.h"
#include "shared/components/Construction.h"
#include "shared/components/Loot.h"
#include "shared/components/NetworkOwner.h"

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

// architecture.md 12.30.8: "a faction that bought your Template can manufacture it forever
// precisely because the design sits in their network -- that is the same gate, applied to them."
// Authored base-game content (never in ContentLibrary's Template overlay) needs no membership --
// every requester may already build it, the same as before this gate existed. A drafted Template
// requires `self`'s own NetworkOwner to hold it, refused (no spend) otherwise.
bool PassesKnowledgeGate(const SystemContext& ctx, entt::entity self,
                        const BlueprintId& blueprint) {
    if (!ctx.content.IsDraftedTemplate(blueprint)) {
        return true;
    }
    if (ctx.knowledge == nullptr) {
        return false;
    }
    const NetworkOwner* owner = ctx.Registry().try_get<NetworkOwner>(self);
    if (owner == nullptr) {
        return false;
    }
    const core::knowledge::KnowledgeNetwork* network = ctx.knowledge->Get(owner->network);
    return network != nullptr && network->savedTemplates.count(blueprint.str()) != 0;
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
        if (!PassesKnowledgeGate(ctx, self, request.blueprint)) {
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
        if (!PassesKnowledgeGate(ctx, self, request.blueprint)) {
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

// architecture.md 12.30.8: the missing half of a five-link chain matching §12.30.6's research
// finding, made worse because the missing datum is already in the intent. BuildSaveRequest's
// SaveTemplateIntent carries the whole ShipBlueprint; this used to grant only
// request.blueprint.id.str() into the network and drop the body, leaving ContentLibrary::FindShip
// with nothing to resolve -- a saved Template could never be built. Fixed by registering the body
// into ContentLibrary's Template overlay (RegisterDraftedTemplate) before granting the network
// entry, so a later BuildStationRequest/PlaceShipRequest naming this id resolves through FindShip
// exactly like authored content does.
//
// Moved here from modes/space/ui/CustomizeMenu.cpp (Law 9: a UI file may not mutate core/ state
// itself) and run from Tick, alongside the request consumers above -- ResearchSystem::Tick
// already calls ctx.knowledge->Grant from a scheduled Tick, so "Law 8 store access is not a
// tick" (this function's old home's stale reasoning) does not hold as a rule.
void ConsumeSaveTemplateRequests(const SystemContext& ctx) {
    if (ctx.knowledge == nullptr || ctx.craftedModules == nullptr) {
        return;  // No store reachable to grant into, or no writable ContentLibrary to register on.
    }
    ctx.intents.ForEach<core::SaveTemplateIntent>([&](const core::SaveTemplateIntent& request) {
        if (!Validate(request.blueprint, ctx.content).ok()) {
            return;
        }
        ctx.craftedModules->RegisterDraftedTemplate(request.blueprint);
        ctx.knowledge->Grant(request.targetNetwork, core::knowledge::NetworkEntryKind::SavedTemplate,
                             request.blueprint.id.str());
    });
}

}  // namespace

void Tick(const SystemContext& ctx) {
    ProcessStationRequests(ctx);
    ProcessShipRequests(ctx);
    ConsumeSaveTemplateRequests(ctx);
}

}  // namespace sr::space::construction_system
