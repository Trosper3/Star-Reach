#include "modes/space/systems/FactionEconomySystem.h"

#include <vector>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "shared/components/Economy.h"

namespace sr::space::faction_economy_system {
namespace {

void ProcessDeposits(entt::registry& registry, core::economy::FactionEconomy& economy) {
    std::vector<entt::entity> consumed;
    for (auto [self, request] : registry.view<DepositRequest>().each()) {
        consumed.push_back(self);
        economy.Deposit(request.faction, request.amount);
    }
    for (const entt::entity self : consumed) {
        registry.remove<DepositRequest>(self);
    }
}

void ProcessSpends(entt::registry& registry, core::economy::FactionEconomy& economy) {
    std::vector<entt::entity> consumed;
    for (auto [self, request] : registry.view<SpendRequest>().each()) {
        consumed.push_back(self);
        const bool succeeded = economy.Spend(request.faction, request.amount);
        registry.emplace_or_replace<SpendResult>(self, succeeded);
    }
    for (const entt::entity self : consumed) {
        registry.remove<SpendRequest>(self);
    }
}

void ProcessTradePressure(entt::registry& registry, core::diplomacy::DiplomacyMatrix& diplomacy) {
    std::vector<entt::entity> consumed;
    for (auto [self, request] : registry.view<TradePressureRequest>().each()) {
        consumed.push_back(self);
        const core::diplomacy::Relation target = request.blockade
                                                     ? core::diplomacy::Relation::Hostile
                                                     : core::diplomacy::Relation::Allied;
        diplomacy.DriftToward(request.a, request.b, target);
    }
    for (const entt::entity self : consumed) {
        registry.remove<TradePressureRequest>(self);
    }
}

void RunTick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    if (ctx.economy != nullptr) {
        ProcessDeposits(registry, *ctx.economy);
        ProcessSpends(registry, *ctx.economy);
    }
    if (ctx.diplomacy != nullptr) {
        ProcessTradePressure(registry, *ctx.diplomacy);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    RunTick(ctx);
}

}  // namespace sr::space::faction_economy_system
