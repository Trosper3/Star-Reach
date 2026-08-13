#include "modes/space/systems/PlayerInputSystem.h"

#include "core/events/Intent.h"
#include "shared/components/Combat.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Targeting.h"

namespace sr::space::player_input_system {
namespace {

// Linear scan over the (single-player: one-entry) ActorRef view. Not a map -- architecture.md
// 12.24 step 2's own words are "single-player has exactly one non-zero actor," and a map would
// be premature machinery for a handful of live actors.
entt::entity Resolve(entt::registry& registry, ActorId actor) {
    for (auto [entity, ref] : registry.view<ActorRef>().each()) {
        if (ref.id == actor) {
            return entity;
        }
    }
    return entt::null;
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    ctx.intents.ForEach<core::SetThrottleIntent>([&](const core::SetThrottleIntent& intent) {
        const entt::entity entity = Resolve(registry, intent.actor);
        if (entity == entt::null) {
            return;  // Unresolvable ActorId: dropped, not asserted (IntentQueue.h's contract).
        }
        if (auto* thrust = registry.try_get<ThrustInput>(entity)) {
            thrust->forward = intent.forward;
            thrust->strafe = intent.strafe;
            thrust->turn = intent.turn;
        }
    });

    ctx.intents.ForEach<core::FireWeaponsIntent>([&](const core::FireWeaponsIntent& intent) {
        const entt::entity entity = Resolve(registry, intent.actor);
        if (entity == entt::null) {
            return;
        }
        registry.emplace_or_replace<FireIntent>(entity);
    });

    ctx.intents.ForEach<core::AimIntent>([&](const core::AimIntent& intent) {
        const entt::entity entity = Resolve(registry, intent.actor);
        if (entity == entt::null) {
            return;
        }
        registry.emplace_or_replace<AimPoint>(entity, AimPoint{intent.worldPosition});
    });

    ctx.intents.ForEach<core::ToggleWeaponGroupIntent>(
        [&](const core::ToggleWeaponGroupIntent& intent) {
            const entt::entity entity = Resolve(registry, intent.actor);
            if (entity == entt::null) {
                return;
            }
            if (auto* groups = registry.try_get<EnabledWeaponGroups>(entity)) {
                groups->mask ^= static_cast<std::uint16_t>(1u << intent.groupIndex);
            }
        });
}

}  // namespace sr::space::player_input_system
