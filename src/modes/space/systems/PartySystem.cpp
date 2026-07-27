#include "modes/space/systems/PartySystem.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "shared/components/Health.h"
#include "shared/components/Party.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"
#include "shared/math/Vec2.h"

namespace sr::space::party_system {
namespace {

// Close enough to a formation slot to stop thrusting -- without this a member arriving exactly
// on the slot would chatter its throttle every tick trying to close a residual few units.
constexpr float kArriveRadiusUnits = 40.0f;
constexpr float kTurnGainPerRadian = 1.0f / kPi;
constexpr float kHeadingToleranceRadians = 0.3f;

bool IsEngaging(const entt::registry& registry, entt::entity self) {
    const auto* target = registry.try_get<Target>(self);
    return target != nullptr && target->rig != entt::null && registry.valid(target->rig);
}

void SteerTowardFormationSlot(const WorldTransform& xf, const WorldTransform& leaderXf,
                              const Vec2& formationOffset, ThrustInput& thrust) {
    const Vec2 slot = leaderXf.position + Rotated(formationOffset, leaderXf.rotation);
    const Vec2 toSlot = slot - xf.position;
    const float distance = Length(toSlot);

    thrust = ThrustInput{};
    if (distance <= kArriveRadiusUnits) {
        return;
    }

    const float headingError = AngleDelta(xf.rotation, ToAngle(toSlot));
    thrust.turn = std::clamp(headingError * kTurnGainPerRadian, -1.0f, 1.0f);
    if (std::abs(headingError) <= kHeadingToleranceRadians) {
        thrust.forward = 1.0f;
    }
}

// The rig root that dealt PendingDamage to any hardpoint of `rig` this tick, entt::null if none.
// Must run before DamageSystem drains and clears PendingDamage, or there is nothing left to read.
entt::entity FindAttacker(const entt::registry& registry, const Rig* rig) {
    if (rig == nullptr) {
        return entt::null;
    }
    for (const entt::entity hardpoint : rig->children) {
        const auto* pending = registry.try_get<PendingDamage>(hardpoint);
        if (pending != nullptr && pending->source != entt::null &&
            registry.valid(pending->source)) {
            return pending->source;
        }
    }
    return entt::null;
}

// Sets `attacker` as the target for every party member not already fighting something, so an
// ambush on one member calls in the rest even if the attacker is outside their own SensorRange.
// Leaves TargetingSystem to pick the actual aim point next tick.
void AlertParty(entt::registry& registry, entt::entity attacker, entt::entity leader,
                const std::vector<entt::entity>& members) {
    auto alert = [&](entt::entity self) {
        auto* target = registry.try_get<Target>(self);
        if (target != nullptr && target->rig == entt::null) {
            target->rig = attacker;
            target->hardpoint = entt::null;
        }
    };
    alert(leader);
    for (const entt::entity member : members) {
        if (registry.valid(member)) {
            alert(member);
        }
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    for (auto [leaderEntity, party] : registry.view<PartyLeader>().each()) {
        entt::entity attacker = entt::null;
        if (const auto* leaderRig = registry.try_get<Rig>(leaderEntity)) {
            attacker = FindAttacker(registry, leaderRig);
        }
        if (attacker == entt::null) {
            for (const entt::entity member : party.members) {
                if (!registry.valid(member)) {
                    continue;
                }
                attacker = FindAttacker(registry, registry.try_get<Rig>(member));
                if (attacker != entt::null) {
                    break;
                }
            }
        }
        if (attacker != entt::null) {
            AlertParty(registry, attacker, leaderEntity, party.members);
        }
    }

    for (auto [member, partyMember, xf, thrust] :
         registry.view<PartyMember, WorldTransform, ThrustInput>().each()) {
        if (IsEngaging(registry, member)) {
            continue;  // Let NpcAiSystem's combat maneuvering stand for this tick.
        }
        if (partyMember.leader == entt::null || !registry.valid(partyMember.leader)) {
            continue;
        }
        const auto* leaderXf = registry.try_get<WorldTransform>(partyMember.leader);
        if (leaderXf == nullptr) {
            continue;
        }
        SteerTowardFormationSlot(xf, *leaderXf, partyMember.formationOffset, thrust);
    }
}

}  // namespace sr::space::party_system
