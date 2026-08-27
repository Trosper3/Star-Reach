#include "modes/space/systems/NpcAiSystem.h"

#include <algorithm>
#include <vector>

#include "shared/components/Ai.h"
#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Identity.h"
#include "shared/components/Orbit.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/StationServices.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"
#include "shared/math/Vec2.h"
#include "shared/rig/ModuleAttachment.h"

namespace sr::space::npc_ai_system {
namespace {

// Stop closing once this near the target's root -- WeaponSystem's own range/arc checks decide
// whether a shot actually connects from there. Closing further just trades range for a worse
// angle. Also the Chase/Attack boundary: within this range is Attack, beyond it is Chase.
constexpr float kEngageRangeUnits = 600.0f;

// Full turn command saturates at a 180-degree heading error and scales down linearly as the nose
// comes onto the target, rather than a bang-bang command that would chatter once roughly aligned.
constexpr float kTurnGainPerRadian = 1.0f / kPi;

// Burn the main engine only once roughly pointed at the target -- thrusting broadside just
// widens the miss, it does not close distance any faster than pointing first would. Shared by
// pursuit, flight, and escort steering -- all three are "turn onto a heading, then burn."
constexpr float kHeadingToleranceRadians = 0.3f;

// Extra clearance kept outside a GravityWell's own `range` before an NPC treats it as a hazard
// to flee rather than terrain to ignore. Pure pursuit otherwise has no reason not to cut straight
// through a well on the way to a target on the far side, and by the time the well's Velocity pull
// (OrbitSystem) is visible in position it is already inside the falloff, well past the point a
// fighter's thrust can climb back out of -- this margin gives it room to turn and burn clear
// before that happens instead of after.
constexpr float kGravityAvoidMarginUnits = 400.0f;

// Below this fraction of aggregate structural integrity (shared/rig/ModuleAttachment.h), a rig
// disengages instead of continuing to fight. Deliberately well above
// rig_attachment::kStructuralFailureThreshold's 0.30 -- a threshold at or below the destruction
// line would never fire, since DamageSystem destroys the rig outright at 0.30 first. Not tuned
// (first pass, the same shape CommanderSystem's own kRetreatHealthFraction carries).
constexpr float kFleeIntegrityFraction = 0.5f;

// How close is "in formation" for Escort -- once within this range of the escorted rig, hold
// position rather than continuing to close. Not tuned.
constexpr float kEscortHoldRangeUnits = 300.0f;

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

// Turns onto the escape heading and burns once aligned if a GravityWell hazard is within its
// avoidance range. Returns true if it took over this tick's steering -- overrides every AI state,
// Flee included: a rig this deep in a well is seconds from Corona/HazardSystem regardless of what
// it is doing, and burning toward danger instead of away from it is how it ends up dead.
bool TryAvoidGravity(const std::vector<GravityHazard>& hazards, const WorldTransform& xf,
                     ThrustInput& thrust) {
    for (const GravityHazard& hazard : hazards) {
        const Vec2 away = xf.position - hazard.position;
        if (Length(away) >= hazard.avoidRange) {
            continue;
        }
        const float headingError = AngleDelta(xf.rotation, ToAngle(away));
        thrust.turn = std::clamp(headingError * kTurnGainPerRadian, -1.0f, 1.0f);
        if (std::abs(headingError) <= kHeadingToleranceRadians) {
            thrust.forward = 1.0f;
        }
        return true;
    }
    return false;
}

// True once `self`'s own aggregate structural integrity has dropped below the Flee threshold. A
// rig with no Rig/Health data at all (nothing to judge structural integrity by) is treated as
// fine rather than as destroyed -- the opposite of
// rig_attachment::AggregateStructuralIntegrity's own "no data" return of 0.0f, which exists for a
// different reader (CockpitHud's display bar) with no such fixture-without-a-hull case to worry
// about.
bool ShouldFlee(const entt::registry& registry, entt::entity self) {
    const auto* rig = registry.try_get<Rig>(self);
    if (rig == nullptr || rig->children.empty()) {
        return false;
    }
    return rig_attachment::AggregateStructuralIntegrity(registry, self) < kFleeIntegrityFraction;
}

// Turns onto the heading directly away from `threatXf` and burns once aligned -- the same
// turn-then-burn shape TryAvoidGravity already uses, aimed the opposite direction (away from a
// threat instead of away from a hazard).
void FleeFrom(const WorldTransform& xf, const WorldTransform& threatXf, ThrustInput& thrust) {
    const Vec2 away = xf.position - threatXf.position;
    if (Length(away) <= 0.0f) {
        return;
    }
    const float headingError = AngleDelta(xf.rotation, ToAngle(away));
    thrust.turn = std::clamp(headingError * kTurnGainPerRadian, -1.0f, 1.0f);
    if (std::abs(headingError) <= kHeadingToleranceRadians) {
        thrust.forward = 1.0f;
    }
}

// Closes on `escortXf` until within kEscortHoldRangeUnits, then holds station. No formation
// offset (that is PartySystem/PartyMember's job, deliberately not used here -- NpcAiSystem.h).
void PursueEscort(const WorldTransform& xf, const WorldTransform& escortXf, ThrustInput& thrust) {
    const Vec2 toEscort = escortXf.position - xf.position;
    if (Length(toEscort) <= kEscortHoldRangeUnits) {
        return;
    }
    const float headingError = AngleDelta(xf.rotation, ToAngle(toEscort));
    thrust.turn = std::clamp(headingError * kTurnGainPerRadian, -1.0f, 1.0f);
    if (std::abs(headingError) <= kHeadingToleranceRadians) {
        thrust.forward = 1.0f;
    }
}

// No hostile Target this tick: Escort if AiBehavior names a live, positioned escortTarget, else
// Patrol. There is no galaxy-level waypoint/rally-point concept yet (architecture.md 12.6's
// NavigationMap, which would supply one, is itself unbuilt), so Patrol is simply coasting.
void PatrolOrEscort(const entt::registry& registry, AiBehavior& behavior, const WorldTransform& xf,
                    ThrustInput& thrust) {
    if (behavior.escortTarget != entt::null && registry.valid(behavior.escortTarget) &&
        registry.all_of<WorldTransform>(behavior.escortTarget)) {
        behavior.state = AiState::Escort;
        PursueEscort(xf, registry.get<WorldTransform>(behavior.escortTarget), thrust);
        return;
    }
    behavior.state = AiState::Patrol;
}

// `self`'s own reach for deciding when a chase is no longer worth continuing -- its SensorRange
// when a Sensor module has actually given it one, else a fallback. No authored ship mounts a
// Sensor module yet (data/base_game/ships.json), so RigFactory's every rig carries SensorRange ==
// 0 in practice today; without the fallback, Chase would never give up on a target that outran
// kEngageRangeUnits, since TargetingSystem's own IsValidTarget never drops a Target for distance
// alone.
constexpr float kFallbackReachUnits = 2000.0f;

float Reach(const SensorRange* sensor) {
    return (sensor != nullptr && sensor->units > 0.0f) ? sensor->units : kFallbackReachUnits;
}

// A live, positioned hostile Target: close distance until within kEngageRangeUnits (Chase), hold
// and request fire once there (Attack too, once aimed -- WeaponSystem's own per-hardpoint
// Weapon::rangeUnits is what actually gates a shot landing, not this system). Beyond `self`'s own
// reach, give up rather than chase forever: clear the Target so TargetingSystem is free to
// acquire a closer one next tick, and fall back to Patrol/Escort.
void Engage(entt::registry& registry, AiBehavior& behavior, entt::entity self, Target& target,
            const WorldTransform& xf, ThrustInput& thrust) {
    const WorldTransform& targetXf = registry.get<WorldTransform>(target.rig);
    const Vec2 toTarget = targetXf.position - xf.position;
    const float distance = Length(toTarget);
    if (distance <= 0.0f) {
        return;
    }

    if (distance > Reach(registry.try_get<SensorRange>(self))) {
        target.rig = entt::null;
        target.hardpoint = entt::null;
        PatrolOrEscort(registry, behavior, xf, thrust);
        return;
    }

    const float headingError = AngleDelta(xf.rotation, ToAngle(toTarget));
    thrust.turn = std::clamp(headingError * kTurnGainPerRadian, -1.0f, 1.0f);

    const bool inEngageRange = distance <= kEngageRangeUnits;
    behavior.state = inEngageRange ? AiState::Attack : AiState::Chase;

    if (std::abs(headingError) <= kHeadingToleranceRadians && !inEngageRange) {
        thrust.forward = 1.0f;
    }

    registry.emplace_or_replace<FireIntent>(self);
}

// One rig's full state-machine evaluation for this tick: reset, dodge, flee, or engage/escort/
// patrol, in that priority order.
void HandleRig(entt::registry& registry, const std::vector<GravityHazard>& hazards,
               entt::entity self, Target& target, WorldTransform& xf, ThrustInput& thrust) {
    // Reset every tick: a rig that lost its target coasts and stops asking to fire, rather than
    // holding whatever throttle and FireIntent it last had.
    thrust = ThrustInput{};
    registry.remove<FireIntent>(self);

    // get_or_emplace, not a view requirement: a rig with no AiBehavior yet (every rig before this
    // system's first tick on it) gets Patrol's default rather than being silently skipped -- the
    // same "absence means the base case, not an error" shape Uncrewed's own recompute already
    // uses.
    AiBehavior& behavior = registry.get_or_emplace<AiBehavior>(self);

    // A hazard dodge overrides every state but is not itself a behavior transition, so
    // AiBehavior is left untouched here.
    if (TryAvoidGravity(hazards, xf, thrust)) {
        return;
    }

    if (ShouldFlee(registry, self)) {
        behavior.state = AiState::Flee;
        if (target.rig != entt::null && registry.valid(target.rig)) {
            if (const auto* threatXf = registry.try_get<WorldTransform>(target.rig)) {
                FleeFrom(xf, *threatXf, thrust);
            }
        }
        // architecture.md 12.30.4: Flee is the disengagement; this gives it a destination.
        // DockPrompt is already computed fresh this tick by DockingSystem, which runs earlier in
        // TickSchedule -- requesting the same bay it names is the same DockRequest idiom
        // AvionicsMenu uses for the player. DockRequest had zero producers anywhere before this.
        if (const DockPrompt* prompt = registry.try_get<DockPrompt>(self)) {
            registry.emplace_or_replace<DockRequest>(self, prompt->bay);
        }
        return;
    }

    const bool hasTarget = target.rig != entt::null && registry.valid(target.rig) &&
                           registry.all_of<WorldTransform>(target.rig);
    if (!hasTarget) {
        PatrolOrEscort(registry, behavior, xf, thrust);
        return;
    }

    Engage(registry, behavior, self, target, xf, thrust);
}

// architecture.md 12.30.4: "who pays when the repaired rig is not the player's" -- an NPC's
// other half. Any docked, damaged, non-player rig orders its own full repair against
// StationServicesSystem (which resolves the payer: Wallet if it has one, ctx.economy otherwise).
// get_or_emplace-shaped, not emplace_or_replace: overwriting an in-progress order every tick
// is needless churn (RepairBilling, not this order, is what actually carries the fractional
// credit remainder now -- StationServices.h's issue #268 comment).
void EmitNpcRepairOrders(entt::registry& registry) {
    for (auto [self, docked, rig] :
         registry.view<Docked, Rig>(entt::exclude<PlayerLocation>).each()) {
        (void)docked;
        (void)rig;
        if (registry.all_of<RepairOrder>(self)) {
            continue;
        }
        if (rig_attachment::AggregateStructuralIntegrity(registry, self) >= 1.0f) {
            continue;
        }
        registry.emplace<RepairOrder>(self, RepairOrder{self, entt::null, 1.0f});
    }
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
    // the sole source of truth, and PlayerControlled is derived from it -- excluding the derived
    // tag here would miss the tick it lags behind, so this excludes PlayerLocation's entity
    // directly, the same entity every other write in this system already keys off.
    //
    // exclude<Uncrewed>: features.md 3.2's uncrewed hull -- a rig whose living control-shell crew
    // is gone has nothing left to steer or shoot with, so it coasts rather than the AI flying and
    // firing a hull nobody is aboard.
    for (auto [self, target, xf, thrust] : registry
                                               .view<Target, WorldTransform, ThrustInput>(
                                                   entt::exclude<PlayerLocation, Docked, Uncrewed>)
                                               .each()) {
        HandleRig(registry, hazards, self, target, xf, thrust);
    }

    EmitNpcRepairOrders(registry);
}

}  // namespace sr::space::npc_ai_system
