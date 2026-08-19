#include "modes/space/systems/DamageSystem.h"

#include <algorithm>

#include "core/registries/DamageTypeEffects.h"
#include "shared/blueprints/Taxonomy.h"
#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Power.h"
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

// Splits `pending` between a matching shield and the hull/power beneath it, per architecture.md
// 12.33's generic damage-type effect table -- a single lookup replaces the old exact-type-match
// branch, and `effect` (the default row for Kinetic/Energy) reproduces that old behavior exactly.
void ApplyToHealthAndShield(entt::registry& registry, entt::entity hardpoint,
                            const PendingDamage& pending, Health& health,
                            const core::DamageTypeEffect& effect) {
    float absorbed = 0.0f;
    bool wasAbsorbed = false;

    auto* shield = registry.try_get<Shield>(hardpoint);
    if (shield != nullptr && shield->current > 0.0f &&
        (shield->absorbs == pending.type || effect.alwaysAbsorbedByAnyShield)) {
        wasAbsorbed = true;
        absorbed = std::min(shield->current, pending.amount);
        shield->current -= absorbed;
        shield->rechargeCooldown = shield->rechargeDelaySeconds;
    }

    const float remaining = pending.amount - absorbed;
    const float hullDamage = remaining * effect.hullDamageFraction;
    health.current = std::max(0.0f, health.current - hullDamage);
    if (health.current <= 0.0f) {
        registry.emplace_or_replace<Destroyed>(hardpoint);
    }

    // Ion's case: absorption protects the hull but does not prevent the power-suppression
    // side effect, so a still-absorbed hit with this flag set drains power off the full
    // incoming amount rather than only whatever "survived" the shield (architecture.md 12.33).
    const float powerBase =
        (wasAbsorbed && effect.bypassStillDrainsShieldCharge) ? pending.amount : remaining;
    const float powerDrain = powerBase * effect.powerDrainFraction;
    if (powerDrain > 0.0f) {
        if (auto* source = registry.try_get<PowerSource>(hardpoint)) {
            source->generation = std::max(0.0f, source->generation - powerDrain);
        }
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
        const core::DamageTypeEffect effect = ctx.content.LookupDamageTypeEffect(pending.type);
        ApplyToHealthAndShield(registry, hardpoint, pending, health, effect);
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
