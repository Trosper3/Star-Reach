#include "modes/space/systems/DamageSystem.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "core/registries/DamageTypeEffects.h"
#include "shared/blueprints/Taxonomy.h"
#include "shared/components/Docking.h"
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

// `satisfaction` is that shield's rig's PowerBudget.shields (1.0 if the hardpoint has no
// ParentRig/PowerBudget to consult, the same "unpowered scenario defaults to full satisfaction"
// convention WeaponSystem::RigSatisfaction already establishes for weapons) -- a browned-out
// shield's post-hit cooldown counts down slower and its regen is slower once it resumes, mirroring
// how WeaponSystem scales cooldown recovery by PowerBudget.weapons. Before this,
// PowerBudget.shields was computed every tick (PowerSystem.cpp) but had no reader at all (Power.h's
// own comment on the field: "shields/facilities... wait on their own consumer") -- this is that
// consumer.
void RegenerateShield(Shield& shield, float dt, float satisfaction) {
    if (shield.rechargeCooldown > 0.0f) {
        shield.rechargeCooldown = std::max(0.0f, shield.rechargeCooldown - dt * satisfaction);
        return;
    }
    shield.current =
        std::min(shield.max, shield.current + shield.rechargePerSecond * dt * satisfaction);
}

float RigShieldSatisfaction(const entt::registry& registry, entt::entity rigRoot) {
    const auto* budget = registry.try_get<PowerBudget>(rigRoot);
    return budget != nullptr ? budget->shields : 1.0f;
}

// Which living, powered Shield on `hardpoint`'s rig actually covers it (architecture.md 12.22) --
// the fix for a generator that used to protect only its own housing regardless of what its
// capacity implied to the player. A shield on the hit hardpoint itself always covers it, in every
// mode; otherwise the rig's other shields are searched in Rig::children order for the first whose
// mode reaches this hardpoint (rig_attachment::ShieldCovers) -- shared with the status
// projection's opposite-direction query (features.md 3.9), which enumerates a shield's coverage
// set rather than a hardpoint's coverer. Skips a PowerShed candidate exactly like WeaponSystem
// skips a shed weapon mount: "a browned-out mount goes offline entirely rather than just running
// at reduced effect" (architecture.md 13.3 finding F) applies to a shield generator the same as a
// gun -- an offline generator projects no field, not a weaker one.
entt::entity FindCoveringShield(const entt::registry& registry, entt::entity hardpoint) {
    if (registry.all_of<Shield>(hardpoint) && !registry.all_of<PowerShed>(hardpoint)) {
        return hardpoint;
    }

    const auto* parent = registry.try_get<ParentRig>(hardpoint);
    const auto* rig = parent != nullptr ? registry.try_get<Rig>(parent->root) : nullptr;
    if (rig == nullptr) {
        return entt::null;
    }

    for (const entt::entity candidate : rig->children) {
        if (candidate == hardpoint || registry.any_of<Destroyed, PowerShed>(candidate)) {
            continue;
        }
        if (registry.all_of<Shield>(candidate) &&
            rig_attachment::ShieldCovers(registry, candidate, hardpoint)) {
            return candidate;
        }
    }
    return entt::null;
}

// Where an absorbed hit's flash should render (ShieldImpactFlash's own comment has the full
// rationale): Personal and Conformal both flash exactly on the hit hardpoint, since Personal only
// ever covers its own hardpoint and Conformal's "hull contact point" is that same hardpoint's
// position; Bubble flashes out at the point on its own coverageRadius circle nearest the hit.
// nullopt when either hardpoint has no WorldTransform -- true of any real spawned rig, but not of
// a unit test's hand-built bare entity, and a missing flash is a fine thing to skip silently
// where a missing Health or Shield would not be.
std::optional<Vec2> ComputeShieldImpactPosition(const entt::registry& registry,
                                                const Shield& shield, entt::entity coveringShield,
                                                entt::entity hitHardpoint) {
    const auto* hitXf = registry.try_get<WorldTransform>(hitHardpoint);
    if (hitXf == nullptr) {
        return std::nullopt;
    }
    if (shield.coverage != ShieldCoverage::Bubble) {
        return hitXf->position;
    }
    const auto* shieldXf = registry.try_get<WorldTransform>(coveringShield);
    if (shieldXf == nullptr) {
        return std::nullopt;
    }
    const Vec2 toHit = hitXf->position - shieldXf->position;
    // A hit landing exactly on the shield generator's own center has no direction to project
    // outward along -- falls back to a fixed direction rather than dividing by zero.
    const Vec2 direction = Length(toHit) > 0.0f ? Normalized(toHit) : Vec2{1.0f, 0.0f};
    return shieldXf->position + direction * shield.coverageRadius;
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
            if (const std::optional<Vec2> impactPosition =
                    ComputeShieldImpactPosition(registry, shield, coveringShield, hardpoint)) {
                // Only Personal's flash shape needs the hit hardpoint's own shell size (see
                // ShieldImpactFlash's comment); left at 0 for every other mode, where
                // WorldRenderer never reads it.
                const auto* hitRadius = registry.try_get<HitRadius>(hardpoint);
                registry.emplace_or_replace<ShieldImpactFlash>(
                    coveringShield,
                    ShieldImpactFlash{
                        .worldPosition = *impactPosition,
                        .type = pending.type,
                        .coverage = shield.coverage,
                        .hardpointRadius = hitRadius != nullptr ? hitRadius->value : 0.0f,
                        .secondsRemaining = kShieldImpactFlashSeconds});
            }
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

// Destroys every rig docked to a host that died this tick -- architecture.md 12.34's "dies with
// its host" half of features.md 3.4, the counterpart to the exclude<Docked> hit-testing half
// fixed elsewhere. Docked.station is a docked rig's own root's window onto its host's root, so a
// dead host is found by checking that reference, not by walking the host's own Rig::children --
// a docked rig is never one of those. Runs immediately after the rig-death loop above, in the same
// tick the host's Destroyed tag is applied, so nothing downstream (LootSystem's next tick) ever
// reads an orphaned Docked.station off a rig this pass should also have condemned. Fixed-point
// the same way CascadeStructuralDestruction is, so a rig docked to a rig just cascade-destroyed
// this same tick still resolves now rather than lagging a tick behind.
//
// Deliberately does nothing else: tagging Destroyed here is what hands a docked rig to LootSystem's
// existing exclude<PlayerLocation> combat-kill sweep (SystemSchedule.cpp runs it after this
// system), the same DeathWreck-and-CargoHold-spill path an ordinary kill already uses -- reusing
// that function with a different cause, not building a parallel one.
void CascadeDockedDestruction(entt::registry& registry) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto [root, docked] : registry.view<Docked>(entt::exclude<Destroyed>).each()) {
            if (docked.station != entt::null && registry.all_of<Destroyed>(docked.station)) {
                registry.remove<Targetable>(root);
                registry.emplace<Destroyed>(root);
                changed = true;
            }
        }
    }
}

// Counts every live ShieldImpactFlash down by dt and drops it once its lifetime expires. Runs
// before this tick's own damage pass so a hit landing THIS tick (which (re-)sets its flash to a
// full kShieldImpactFlashSeconds via emplace_or_replace) is never aged down in the same tick it
// was created. Collecting expired entities first rather than removing mid-iteration: entt's view
// iteration over a component's own storage is not safe to erase from as it goes.
void AgeShieldImpactFlashes(entt::registry& registry, float dt) {
    std::vector<entt::entity> expired;
    for (auto [entity, flash] : registry.view<ShieldImpactFlash>().each()) {
        flash.secondsRemaining -= dt;
        if (flash.secondsRemaining <= 0.0f) {
            expired.push_back(entity);
        }
    }
    for (const entt::entity entity : expired) {
        registry.remove<ShieldImpactFlash>(entity);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    AgeShieldImpactFlashes(registry, ctx.dt);

    // exclude<PowerShed>: an offline generator does not passively recharge either --
    // RegenerateShield's own comment.
    for (auto [hardpoint, shield] :
         registry.view<Shield>(entt::exclude<Destroyed, PowerShed>).each()) {
        const auto* parent = registry.try_get<ParentRig>(hardpoint);
        const float satisfaction =
            parent != nullptr ? RigShieldSatisfaction(registry, parent->root) : 1.0f;
        RegenerateShield(shield, ctx.dt, satisfaction);
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

    CascadeDockedDestruction(registry);
}

}  // namespace sr::space::damage_system
