#include "modes/space/systems/FactionEconomySystem.h"

#include <vector>

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

void RunTick(const SystemContext& ctx) {
    if (ctx.economy == nullptr) {
        return;
    }
    entt::registry& registry = ctx.Registry();
    ProcessDeposits(registry, *ctx.economy);
    ProcessSpends(registry, *ctx.economy);
}

}  // namespace

void Tick(const SystemContext& ctx) {
    RunTick(ctx);
}

void TickCoarse(const SystemContext& ctx) {
    RunTick(ctx);
}

}  // namespace sr::space::faction_economy_system
