#include "modes/space/systems/DiscoverySystem.h"

#include "shared/components/Identity.h"

namespace sr::space::discovery_system {
namespace {

void RunTick(const SystemContext& ctx) {
    if (ctx.discovery == nullptr) {
        return;
    }
    const entt::registry& registry = ctx.Registry();
    for (auto [entity, faction] : registry.view<PlayerControlled, FactionRef>().each()) {
        (void)entity;
        ctx.discovery->Discover(faction.id, ctx.world.SystemId());
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    RunTick(ctx);
}

void TickCoarse(const SystemContext& ctx) {
    RunTick(ctx);
}

}  // namespace sr::space::discovery_system
