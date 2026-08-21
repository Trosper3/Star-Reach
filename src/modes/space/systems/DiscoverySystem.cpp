#include "modes/space/systems/DiscoverySystem.h"

#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/systems/PlayerRecordSystem.h"

namespace sr::space::discovery_system {
namespace {

void RunTick(const SystemContext& ctx) {
    if (ctx.knowledge == nullptr) {
        return;
    }
    // The player's own FactionId, not whatever hull PlayerControlled currently names -- docked at
    // a foreign station, that hull's FactionRef is the host's, and discovery must not credit them
    // (architecture.md 12.30.3, amending 12.30.1).
    const FactionId faction = player_record_system::FactionOf(ctx.Registry());
    if (faction.empty()) {
        return;
    }
    ctx.knowledge->Grant(core::knowledge::FactionNetworkId(faction),
                         core::knowledge::NetworkEntryKind::DiscoveredSystem, ctx.world.SystemId());
}

}  // namespace

void Tick(const SystemContext& ctx) {
    RunTick(ctx);
}

}  // namespace sr::space::discovery_system
