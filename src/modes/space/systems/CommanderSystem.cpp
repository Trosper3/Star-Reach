#include "modes/space/systems/CommanderSystem.h"

#include "shared/components/Commander.h"
#include "shared/components/Health.h"
#include "shared/components/Rig.h"

namespace sr::space::commander_system {
namespace {

// Placeholder, not tuned (see CommanderSystem.h's comment): below this fraction of aggregate
// hull, a commander not already retreating breaks off.
constexpr float kRetreatHealthFraction = 0.25f;

bool IsBadlyDamaged(const entt::registry& registry, entt::entity commanderRoot) {
    const Rig* rig = registry.try_get<Rig>(commanderRoot);
    if (rig == nullptr) {
        return false;
    }

    float totalCurrent = 0.0f;
    float totalMax = 0.0f;
    for (const entt::entity child : rig->children) {
        if (const Health* health = registry.try_get<Health>(child)) {
            totalCurrent += health->current;
            totalMax += health->max;
        }
    }

    return totalMax > 0.0f && (totalCurrent / totalMax) < kRetreatHealthFraction;
}

void Resolve(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    for (auto [entity, commander] : registry.view<Commander>().each()) {
        if (commander.orders != CommanderOrders::Retreat && IsBadlyDamaged(registry, entity)) {
            commander.orders = CommanderOrders::Retreat;
        }
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    Resolve(ctx);
}

}  // namespace sr::space::commander_system
