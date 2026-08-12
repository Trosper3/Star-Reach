#include "modes/space/systems/DamageSystem.h"

#include <algorithm>

#include "shared/blueprints/Taxonomy.h"
#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/rig/ModuleAttachment.h"

namespace sr::space::damage_system {
namespace {

void RegenerateShield(Shield& shield, float dt) {
    if (shield.rechargeCooldown > 0.0f) {
        shield.rechargeCooldown = std::max(0.0f, shield.rechargeCooldown - dt);
        return;
    }
    shield.current = std::min(shield.max, shield.current + shield.rechargePerSecond * dt);
}

// Splits `pending` between a matching shield and the hull beneath it, per features.md section
// 3.1: matching damage type is absorbed by the shield first; a mismatch bypasses it entirely.
void ApplyToHealthAndShield(entt::registry& registry, entt::entity hardpoint,
                            const PendingDamage& pending, Health& health) {
    float hullDamage = pending.amount;

    auto* shield = registry.try_get<Shield>(hardpoint);
    if (shield != nullptr && shield->current > 0.0f && shield->absorbs == pending.type) {
        const float absorbed = std::min(shield->current, pending.amount);
        shield->current -= absorbed;
        shield->rechargeCooldown = shield->rechargeDelaySeconds;
        hullDamage -= absorbed;
    }

    health.current = std::max(0.0f, health.current - hullDamage);
    if (health.current <= 0.0f) {
        registry.emplace_or_replace<Destroyed>(hardpoint);
    }
}

bool HasLivingHardpoint(const entt::registry& registry, const Rig& rig) {
    for (const entt::entity child : rig.children) {
        if (registry.all_of<Health>(child) && !registry.all_of<Destroyed>(child)) {
            return true;
        }
    }
    return false;
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    for (auto [hardpoint, shield] : registry.view<Shield>(entt::exclude<Destroyed>).each()) {
        RegenerateShield(shield, ctx.dt);
    }

    for (auto [hardpoint, pending, health] : registry.view<PendingDamage, Health>().each()) {
        ApplyToHealthAndShield(registry, hardpoint, pending, health);
    }
    registry.clear<PendingDamage>();

    for (auto [root, rig] : registry.view<Rig>().each()) {
        if (!HasLivingHardpoint(registry, rig)) {
            registry.remove<Targetable>(root);
            registry.emplace_or_replace<Destroyed>(root);
            continue;
        }

        // Unconditional, not gated on "did an engine just die": recomputing every tick is what
        // makes losing one of several engines cost thrust proportionally, rather than the old
        // all-or-nothing HasLivingEngine check that only zeroed Propulsion when the LAST engine
        // died (architecture.md 12.23's Sum/Max rule).
        rig_attachment::RecomputeRigTotals(registry, root);
    }
}

}  // namespace sr::space::damage_system
