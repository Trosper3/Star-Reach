#pragma once

#include <entt/entity/entity.hpp>

#include "shared/blueprints/Taxonomy.h"
#include "shared/math/Vec2.h"

namespace sr {

// Hull of a single hardpoint entity. There is no rig-wide health bar anywhere in this project:
// a rig dies when its hardpoints do (features.md section 3.2, and the capital-ship rule that
// there is no protected core).
struct Health {
    float current = 0.0f;
    float max = 0.0f;
};

// Shield state, on the hardpoint carrying the shield generator module.
//
// Matching damage is absorbed; mismatched damage bypasses entirely and lands on the hull
// beneath (features.md section 3.1). Destroying this hardpoint sets regeneration off
// permanently for the rig -- DamageSystem does not reconstitute it. `coverage`/`coverageRadius`
// are copied from ShieldStats at attach time (architecture.md 12.22): DamageSystem resolves which
// living Shield on the rig actually covers a damaged hardpoint before checking `absorbs`, rather
// than assuming it is always this component's own housing.
struct Shield {
    float current = 0.0f;
    float max = 0.0f;
    DamageType absorbs = DamageType::Kinetic;
    float rechargePerSecond = 0.0f;
    float rechargeDelaySeconds = 0.0f;
    // Counts down after each absorbed hit; recharge resumes only at zero.
    float rechargeCooldown = 0.0f;
    ShieldCoverage coverage = ShieldCoverage::Personal;
    float coverageRadius = 0.0f;
};

// A shield's most recent absorbed hit, for WorldRenderer's flash -- features.md 3.5's "in-world
// shield shimmer" (shared/ui/HudTheme.h's DamageTypeColor comment: "eventually... not yet built"),
// scoped here to the per-hit feedback rather than the full persistent charge-dashed coverage loop
// that section also specifies, which remains unbuilt. `worldPosition` is the coverage-mode-
// specific point features.md 3.1/3.5 describe -- Personal and Conformal both flash exactly where
// the hit landed (Personal only ever covers its own hardpoint; Conformal wraps the whole hull, so
// "the hull contact point" is simply that hardpoint's position too), while Bubble flashes out at
// the point on its own coverageRadius circle nearest the hit, since that is where the field
// actually intercepts something headed for a hardpoint it protects at a distance. Computed once by
// DamageSystem, not re-derived from a moving hull every frame -- a flash this short-lived is
// allowed to lag a fast-turning rig rather than costing a per-frame recomputation. `coverage` and
// `hardpointRadius` (the hit hardpoint's own HitRadius, Personal only -- 0 and unused otherwise)
// let WorldRenderer draw a genuinely different shape per mode -- a ring hugging the hit
// hardpoint's own shell for Personal, a filled burst at the hull contact point for Conformal, and
// a ring rippling outward from the bubble's own perimeter for Bubble -- rather than one shape that
// only differs by position.
// `secondsRemaining` counts down to 0 (DamageSystem ages every live flash down by ctx.dt each
// tick) and removes the component; a fresh absorbed hit always resets it back to
// kShieldImpactFlashSeconds via emplace_or_replace.
constexpr float kShieldImpactFlashSeconds = 0.2f;

struct ShieldImpactFlash {
    Vec2 worldPosition{};
    DamageType type = DamageType::Kinetic;
    ShieldCoverage coverage = ShieldCoverage::Personal;
    float hardpointRadius = 0.0f;
    float secondsRemaining = kShieldImpactFlashSeconds;
};

// Queued damage, written by weapon/collision resolution and drained by DamageSystem.
//
// Damage is a queue rather than an immediate call so that shield typing, bypass, and
// destruction all resolve in one place at one point in the tick. It is also what keeps Law 12
// satisfiable: DamageSystem is the single emitter of destruction consequences.
struct PendingDamage {
    float amount = 0.0f;
    DamageType type = DamageType::Kinetic;
    // The rig root that dealt it, for retaliation and for the relation write in
    // features.md section 5.3. May be entt::null for environmental damage.
    entt::entity source = entt::null;
};

}  // namespace sr
