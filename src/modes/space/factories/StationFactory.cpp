#include "modes/space/factories/StationFactory.h"

#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/NetworkOwner.h"
#include "shared/components/Research.h"
#include "shared/components/Spawn.h"

namespace sr::space::station_factory {

SpawnResult Spawn(SystemWorld& world, const core::ContentLibrary& content,
                  const SpawnParams& params) {
    const ShipBlueprint* blueprint = content.FindShip(params.blueprint);
    if (blueprint == nullptr || blueprint->mobile) {
        return {};
    }

    const SpawnResult result = rig_factory::Spawn(world, content, params);
    if (!result.ok()) {
        return result;
    }

    // The four components architecture.md 13.3 O and 12.30.6 record as having zero producers
    // anywhere -- CargoHold is not among them: it is emergent from a mounted CargoBay module
    // (shared/rig/ModuleAttachment.cpp), the same as for any other rig, per the settled "hold
    // lives on the bay" design -- a station's content simply needs a cargo-bay mount, not a
    // hand-emplaced component here.
    entt::registry& registry = world.Registry();
    registry.emplace<StationFacility>(result.root);
    registry.emplace<SpawnAnchor>(result.root);
    registry.emplace<Wallet>(result.root);

    // Placeholder network id, one per faction rather than per station. There is no KnowledgeStore
    // reachable from a factory (architecture.md 12.24 step 6's ctx.knowledge is still nullptr, and
    // WorldGen/StationFactory take no such pointer), so this cannot call KnowledgeStore::Create()
    // for a real registered network. Deriving the id from FactionId is stable and matches
    // NetworkOwnerKind::Faction's intended shape -- a faction's stations share one general network
    // -- and costs nothing to correct later: whatever bootstraps the real store just needs to
    // Create() (or migrate in) a network under this same id.
    const FactionRef& faction = registry.get<FactionRef>(result.root);
    registry.emplace<NetworkOwner>(result.root, KnowledgeNetworkId(faction.id.str()));

    return result;
}

}  // namespace sr::space::station_factory
