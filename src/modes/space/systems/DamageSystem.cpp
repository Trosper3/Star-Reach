#include "modes/space/systems/DamageSystem.h"

#include <algorithm>

#include "core/registries/DamageTypeEffects.h"
#include "shared/blueprints/Taxonomy.h"
#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"
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

// Which living Shield on `hardpoint`'s rig actually covers it (architecture.md 12.22) -- the fix
// for a generator that used to protect only its own housing regardless of what its capacity
// implied to the player. A shield on the hit hardpoint itself always covers it, in every mode;
// otherwise the rig's other shields are searched in Rig::children order for the first whose mode
// reaches this hardpoint: Conformal unconditionally, Bubble within coverageRadius of its mount. A
// Personal shield elsewhere on the rig never reaches past its own housing, so it is skipped here
// exactly as every shield was before this function existed.
entt::entity FindCoveringShield(const entt::registry& registry, entt::entity hardpoint) {
    if (registry.all_of<Shield>(hardpoint)) {
        return hardpoint;
    }

    const auto* parent = registry.try_get<ParentRig>(hardpoint);
    const auto* rig = parent != nullptr ? registry.try_get<Rig>(parent->root) : nullptr;
    if (rig == nullptr) {
        return entt::null;
    }
    const auto* targetXf = registry.try_get<WorldTransform>(hardpoint);

    for (const entt::entity candidate : rig->children) {
        if (candidate == hardpoint || registry.all_of<Destroyed>(candidate)) {
            continue;
        }
        const auto* shield = registry.try_get<Shield>(candidate);
        if (shield == nullptr) {
            continue;
        }
        if (shield->coverage == ShieldCoverage::Conformal) {
            return candidate;
        }
        if (shield->coverage == ShieldCoverage::Bubble && targetXf != nullptr) {
            const auto* shieldXf = registry.try_get<WorldTransform>(candidate);
            if (shieldXf != nullptr &&
                Distance(shieldXf->position, targetXf->position) <= shield->coverageRadius) {
                return candidate;
            }
        }
    }
    return entt::null;
}

// Splits `pending` between the shield covering this hardpoint (architecture.md 12.22) and the
// hull/power beneath it, per architecture.md 12.33's generic damage-type effect table -- a single
// lookup replaces the old exact-type-match branch, and `effect` (the default row for
// Kinetic/Energy) reproduces that old behavior exactly.
void ApplyToHealthAndShield(entt::registry& registry, entt::entity hardpoint,
                            const PendingDamage& pending, Health& health,
                            const core::DamageTypeEffect& effect) {
    float absorbed = 0.0f;
    bool wasAbsorbed = false;

    const entt::entity coveringShield = FindCoveringShield(registry, hardpoint);
    if (coveringShield != entt::null) {
        auto& shield = registry.get<Shield>(coveringShield);
        if (shield.current > 0.0f &&
            (shield.absorbs == pending.type || effect.alwaysAbsorbedByAnyShield)) {
            wasAbsorbed = true;
            absorbed = std::min(shield.current, pending.amount);
            shield.current -= absorbed;
            shield.rechargeCooldown = shield.rechargeDelaySeconds;
        }
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

// features.md 3.2: below rig_attachment::kStructuralFailureThreshold the hull gives way --
// every surviving hardpoint is destroyed along with the root, not just whichever ones happen to
// have already reached zero individually. A rig with every hardpoint already gone (the old
// HasLivingHardpoint check this replaces) is the degenerate case: its aggregate integrity is
// already 0, so it always falls through the same path.
void ApplyStructuralFailure(entt::registry& registry, entt::entity root, const Rig& rig) {
    for (const entt::entity child : rig.children) {
        if (registry.all_of<Health>(child) && !registry.all_of<Destroyed>(child)) {
            registry.emplace<Destroyed>(child);
        }
    }
    registry.remove<Targetable>(root);
    registry.emplace_or_replace<Destroyed>(root);
}

// Destroys every hardpoint structurally attached, directly or transitively, to one that died
// this tick. StructuralAttachment is already the mount hierarchy graph (RigFactory's
// ResolveAttachments); this is what makes severing a parent take its children with it, and --
// with no special case -- makes chassis death rig death, since everything ultimately traces back
// to the chassis (architecture.md 12.22). Iterates to a fixed point: one pass can newly-destroy a
// child whose own children were already visited earlier in the same pass.
void CascadeStructuralDestruction(entt::registry& registry) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto [hardpoint, attachment] :
             registry.view<StructuralAttachment>(entt::exclude<Destroyed>).each()) {
            if (attachment.attachedTo != entt::null &&
                registry.all_of<Destroyed>(attachment.attachedTo)) {
                registry.emplace<Destroyed>(hardpoint);
                changed = true;
            }
        }
    }
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

    CascadeStructuralDestruction(registry);

    for (auto [root, rig] : registry.view<Rig>().each()) {
        if (rig_attachment::AggregateStructuralIntegrity(registry, root) <
            rig_attachment::kStructuralFailureThreshold) {
            ApplyStructuralFailure(registry, root, rig);
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
