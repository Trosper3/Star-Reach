#include "modes/space/systems/DistressSystem.h"

#include <vector>

#include "shared/components/Distress.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

namespace sr::space::distress_system {
namespace {

// At most one DistressCall entity ever exists at a time -- callers rely on that invariant rather
// than re-checking it themselves.
entt::entity FindActiveCall(entt::registry& registry) {
    const auto view = registry.view<DistressCall>();
    return view.empty() ? entt::null : *view.begin();
}

void GenerateRequests(entt::registry& registry) {
    std::vector<entt::entity> consumed;
    for (auto [self, request] : registry.view<DistressCallRequest>().each()) {
        consumed.push_back(self);
        if (FindActiveCall(registry) == entt::null) {
            const entt::entity call = registry.create();
            registry.emplace<DistressCall>(call, request.call);
        }
    }
    for (const entt::entity self : consumed) {
        registry.remove<DistressCallRequest>(self);
    }
}

void AcknowledgeRequests(entt::registry& registry) {
    const entt::entity callEntity = FindActiveCall(registry);

    std::vector<entt::entity> consumed;
    for (auto [self, xf, sensor] :
         registry.view<AcknowledgeDistressRequest, WorldTransform, SensorRange>().each()) {
        consumed.push_back(self);
        if (callEntity == entt::null) {
            continue;
        }
        DistressCall& call = registry.get<DistressCall>(callEntity);
        if (call.acknowledged) {
            continue;
        }
        if (Distance(xf.position, call.position) <= sensor.units) {
            call.acknowledged = true;
            call.responder = self;
        }
    }
    for (const entt::entity self : consumed) {
        registry.remove<AcknowledgeDistressRequest>(self);
    }
}

void Resolve(entt::registry& registry, float dt) {
    const entt::entity callEntity = FindActiveCall(registry);
    if (callEntity == entt::null) {
        return;
    }
    DistressCall& call = registry.get<DistressCall>(callEntity);

    // ShipUnderAttack fails immediately, regardless of the timer, the moment its target dies.
    if (call.type == DistressType::ShipUnderAttack && call.npcTarget != entt::null &&
        (!registry.valid(call.npcTarget) || registry.all_of<Destroyed>(call.npcTarget))) {
        registry.destroy(callEntity);
        return;
    }

    call.timer += dt;
    if (call.timer < call.windowSeconds) {
        return;  // Still running.
    }

    // Both types define "survived to the window" as success; Stranded's survival condition IS
    // acknowledgment (help arrived in time), so `acknowledged` doubles as both the success
    // condition and the reward gate there. ShipUnderAttack's survival was already confirmed
    // above, so it only needs the second half here -- there being someone to actually pay.
    const bool succeeded = call.type == DistressType::Stranded ? call.acknowledged : true;
    if (succeeded && call.acknowledged) {
        registry.get_or_emplace<Wallet>(call.responder).credits += call.rewardCredits;
    }
    registry.destroy(callEntity);
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    GenerateRequests(registry);
    AcknowledgeRequests(registry);
    Resolve(registry, ctx.dt);
}

}  // namespace sr::space::distress_system
