#pragma once

#include <entt/entity/entity.hpp>

#include "shared/blueprints/Taxonomy.h"

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
// permanently for the rig -- DamageSystem does not reconstitute it.
struct Shield {
    float current = 0.0f;
    float max = 0.0f;
    DamageType absorbs = DamageType::Kinetic;
    float rechargePerSecond = 0.0f;
    float rechargeDelaySeconds = 0.0f;
    // Counts down after each absorbed hit; recharge resumes only at zero.
    float rechargeCooldown = 0.0f;
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
