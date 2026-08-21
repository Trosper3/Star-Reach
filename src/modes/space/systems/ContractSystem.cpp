#include "modes/space/systems/ContractSystem.h"

#include <vector>

#include "core/diplomacy/Reputation.h"
#include "shared/components/Contract.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"

namespace sr::space::contract_system {
namespace {

// features.md 5.3's "positive / negative to the issuer" -- the same magnitude either direction,
// sign flipped by outcome. Reputation.h's own decay (1/sec) and TemplateMarketSystem's ±100 rate
// bonus clamp set the scale this needs to sit inside: large enough to read as a real move, small
// enough that a handful of contracts can't single-handedly swing a faction from Neutral to Allied.
constexpr float kContractRelationStep = 10.0f;

void AdjustIssuerRelation(const SystemContext& ctx, const FactionId& issuingFaction, float delta) {
    if (ctx.reputation == nullptr || issuingFaction.empty()) {
        return;
    }
    ctx.reputation->Adjust(issuingFaction, delta);
}

void AcceptRequests(entt::registry& registry) {
    std::vector<entt::entity> consumed;
    for (auto [holder, request] : registry.view<AcceptContractRequest>().each()) {
        consumed.push_back(holder);
        if (!registry.all_of<Contract>(holder)) {
            registry.emplace<Contract>(holder, request.contract);
        }
    }
    for (const entt::entity holder : consumed) {
        registry.remove<AcceptContractRequest>(holder);
    }
}

// Every faction that had a rig root destroyed this tick, exactly once per corpse ever --
// KillCredited stops a lingering Destroyed tag (nothing removes a destroyed rig root today) from
// being recounted on every future tick.
std::vector<FactionId> CollectFreshKills(entt::registry& registry) {
    std::vector<FactionId> factions;
    for (auto [corpse, faction] :
         registry.view<FactionRef, Destroyed>(entt::exclude<KillCredited>).each()) {
        factions.push_back(faction.id);
        registry.emplace<KillCredited>(corpse);
    }
    return factions;
}

void CreditReward(entt::registry& registry, entt::entity holder, int credits) {
    registry.get_or_emplace<Wallet>(holder).credits += credits;
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    AcceptRequests(registry);
    const std::vector<FactionId> freshKills = CollectFreshKills(registry);

    std::vector<entt::entity> resolved;
    for (auto [holder, contract] : registry.view<Contract>().each()) {
        if (contract.type == ContractType::Bounty) {
            for (const FactionId& faction : freshKills) {
                if (faction == contract.targetFaction) {
                    ++contract.killsDone;
                }
            }
            if (contract.killsDone >= contract.killsRequired) {
                CreditReward(registry, holder, contract.rewardCredits);
                AdjustIssuerRelation(ctx, contract.issuingFaction, kContractRelationStep);
                resolved.push_back(holder);
            }
            continue;
        }

        // Escort.
        if (contract.escortTarget == entt::null || !registry.valid(contract.escortTarget) ||
            registry.all_of<Destroyed>(contract.escortTarget)) {
            AdjustIssuerRelation(ctx, contract.issuingFaction, -kContractRelationStep);
            resolved.push_back(holder);  // Failed -- no reward.
            continue;
        }
        contract.timeRemaining -= ctx.dt;
        if (contract.timeRemaining <= 0.0f) {
            CreditReward(registry, holder, contract.rewardCredits);
            AdjustIssuerRelation(ctx, contract.issuingFaction, kContractRelationStep);
            resolved.push_back(holder);
        }
    }

    for (const entt::entity holder : resolved) {
        registry.remove<Contract>(holder);
    }
}

}  // namespace sr::space::contract_system
