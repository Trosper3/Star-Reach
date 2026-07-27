#include "modes/space/systems/NpcAiSystem.h"

#include <algorithm>

#include "shared/components/Combat.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"
#include "shared/math/Vec2.h"

namespace sr::space::npc_ai_system {
namespace {

// Stop closing once this near the target's root -- WeaponSystem's own range/arc checks decide
// whether a shot actually connects from there. Closing further just trades range for a worse
// angle.
constexpr float kEngageRangeUnits = 600.0f;

// Full turn command saturates at a 180-degree heading error and scales down linearly as the nose
// comes onto the target, rather than a bang-bang command that would chatter once roughly aligned.
constexpr float kTurnGainPerRadian = 1.0f / kPi;

// Burn the main engine only once roughly pointed at the target -- thrusting broadside just
// widens the miss, it does not close distance any faster than pointing first would.
constexpr float kHeadingToleranceRadians = 0.3f;

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    for (auto [self, target, xf, thrust] :
         registry.view<Target, WorldTransform, ThrustInput>(entt::exclude<PlayerControlled>)
             .each()) {
        // Reset every tick: a rig that lost its target coasts and stops asking to fire, rather
        // than holding whatever throttle and FireIntent it last had.
        thrust = ThrustInput{};
        registry.remove<FireIntent>(self);

        if (target.rig == entt::null || !registry.valid(target.rig)) {
            continue;
        }
        const auto* targetXf = registry.try_get<WorldTransform>(target.rig);
        if (targetXf == nullptr) {
            continue;
        }

        const Vec2 toTarget = targetXf->position - xf.position;
        const float distance = Length(toTarget);
        if (distance <= 0.0f) {
            continue;
        }

        const float headingError = AngleDelta(xf.rotation, ToAngle(toTarget));
        thrust.turn = std::clamp(headingError * kTurnGainPerRadian, -1.0f, 1.0f);

        if (std::abs(headingError) <= kHeadingToleranceRadians && distance > kEngageRangeUnits) {
            thrust.forward = 1.0f;
        }

        registry.emplace_or_replace<FireIntent>(self);
    }
}

}  // namespace sr::space::npc_ai_system
