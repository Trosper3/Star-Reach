#include "modes/space/systems/HazardSystem.h"

#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"

namespace sr::space::hazard_system {
namespace {

// Accumulates rather than overwrites, matching ProjectileSystem/CollisionSystem's QueueDamage --
// but the hazard runs first in the schedule (HazardSystem.h), so if a real attacker also lands
// this tick, its QueueDamage call overwrites type/source afterward and wins, exactly as intended.
void QueueDamage(entt::registry& registry, entt::entity entity, float amount) {
    if (auto* pending = registry.try_get<PendingDamage>(entity)) {
        pending->amount += amount;
        pending->type = DamageType::Energy;
        pending->source = entt::null;
    } else {
        registry.emplace<PendingDamage>(entity, amount, DamageType::Energy, entt::null);
    }
}

// features.md 3.2's localized damage model applied honestly: a hazard is a volume, hardpoints and
// asteroids are entities with positions, so each burns according to its own distance from the
// source -- a capital hull half inside the corona burns only on the side that is in.
void ApplyCorona(entt::registry& registry, const Vec2& sourcePosition, const Corona& corona,
                 float dt) {
    for (auto [entity, health, xf] :
         registry.view<Health, WorldTransform>(entt::exclude<Destroyed>).each()) {
        (void)health;
        const float distance = Distance(xf.position, sourcePosition);
        if (distance >= corona.range) {
            continue;
        }
        const float falloff = 1.0f - (distance / corona.range);
        QueueDamage(registry, entity, corona.damagePerSecond * falloff * falloff * dt);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    for (auto [source, corona, sourceXf] : registry.view<Corona, WorldTransform>().each()) {
        (void)source;
        ApplyCorona(registry, sourceXf.position, corona, ctx.dt);
    }
}

}  // namespace sr::space::hazard_system
