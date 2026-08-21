#include "modes/space/systems/DiscoverySystem.h"

#include "core/knowledge/KnowledgeNetwork.h"
#include "shared/components/Identity.h"

namespace sr::space::discovery_system {
namespace {

void RunTick(const SystemContext& ctx) {
    if (ctx.knowledge == nullptr) {
        return;
    }
    const entt::registry& registry = ctx.Registry();
    for (auto [entity, faction] : registry.view<PlayerControlled, FactionRef>().each()) {
        (void)entity;
        ctx.knowledge->Grant(core::knowledge::FactionNetworkId(faction.id),
                             core::knowledge::NetworkEntryKind::DiscoveredSystem,
                             ctx.world.SystemId());
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    RunTick(ctx);
}

}  // namespace sr::space::discovery_system
