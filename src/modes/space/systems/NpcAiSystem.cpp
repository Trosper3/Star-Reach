#include "modes/space/systems/NpcAiSystem.h"

#include <algorithm>
#include <vector>

#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Identity.h"
#include "shared/components/Orbit.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
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

// Extra clearance kept outside a GravityWell's own `range` before an NPC treats it as a hazard
// to flee rather than terrain to ignore. Pure pursuit otherwise has no reason not to cut straight
// through a well on the way to a target on the far side, and by the time the well's Velocity pull
// (OrbitSystem) is visible in position it is already inside the falloff, well past the point a
// fighter's thrust can climb back out of -- this margin gives it room to turn and burn clear
// before that happens instead of after.
constexpr float kGravityAvoidMarginUnits = 400.0f;

// A GravityWell hazard's danger radius, resolved once per tick from every well in the registry
// rather than re-queried per rig.
struct GravityHazard {
    Vec2 position;
    float avoidRange;
};

std::vector<GravityHazard> CollectGravityHazards(const entt::registry& registry) {
    std::vector<GravityHazard> hazards;
    for (auto [wellEntity, well, wellXf] : registry.view<GravityWell, WorldTransform>().each()) {
        hazards.push_back(GravityHazard{wellXf.position, well.range + kGravityAvoidMarginUnits});
    }
    return hazards;
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    const std::vector<GravityHazard> hazards = CollectGravityHazards(registry);

    // exclude<Docked>: DockingSystem zeroes a docked rig's Velocity/ThrustInput once, on dock --
    // without this, NpcAiSystem would rewrite ThrustInput and re-emplace FireIntent every tick
    // after, and a docked NPC would thrust out of the bay and keep firing (architecture.md 13.3
    // finding H).
    //
    // exclude<PlayerLocation>, not PlayerControlled: architecture.md 12.30.1 makes PlayerLocation
    // the sole source of truth today, and nothing derives PlayerControlled yet (P4-01, still
    // open) -- a PlayerControlled-gated exclusion here was a no-op, so this loop was zeroing the
    // player's own ThrustInput and stripping their FireIntent every tick, right before
    // WeaponSystem read it. Movement still looked fine (PhysicsSystem consumes ThrustInput before
    // this system runs), but firing was silently dead regardless of aim.
    //
    // exclude<Uncrewed>: features.md 3.2's uncrewed hull -- a rig whose living control-shell crew
    // is gone has nothing left to steer or shoot with, so it coasts and stops asking to fire the
    // same way a target-less rig already does below, rather than the AI flying and firing a hull
    // nobody is aboard.
    for (auto [self, target, xf, thrust] : registry
                                               .view<Target, WorldTransform, ThrustInput>(
                                                   entt::exclude<PlayerLocation, Docked, Uncrewed>)
                                               .each()) {
        // Reset every tick: a rig that lost its target coasts and stops asking to fire, rather
        // than holding whatever throttle and FireIntent it last had.
        thrust = ThrustInput{};
        registry.remove<FireIntent>(self);

        // Gravity avoidance overrides pursuit entirely, no FireIntent included: a rig this deep
        // in a well is seconds from Corona/HazardSystem regardless of what it is chasing, and
        // burning the main engine toward the target instead of away is how it ends up dead.
        bool fleeingGravity = false;
        for (const GravityHazard& hazard : hazards) {
            const Vec2 away = xf.position - hazard.position;
            const float distance = Length(away);
            if (distance >= hazard.avoidRange) {
                continue;
            }
            const float escapeHeadingError = AngleDelta(xf.rotation, ToAngle(away));
            thrust.turn = std::clamp(escapeHeadingError * kTurnGainPerRadian, -1.0f, 1.0f);
            if (std::abs(escapeHeadingError) <= kHeadingToleranceRadians) {
                thrust.forward = 1.0f;
            }
            fleeingGravity = true;
            break;
        }
        if (fleeingGravity) {
            continue;
        }

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
