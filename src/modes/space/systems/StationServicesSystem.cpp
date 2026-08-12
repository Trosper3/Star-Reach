#include "modes/space/systems/StationServicesSystem.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "shared/components/Docking.h"
#include "shared/components/Health.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/components/StationServices.h"

namespace sr::space::station_services_system {
namespace {

entt::entity DockedStation(const entt::registry& registry, entt::entity requester) {
    const Docked* docked = registry.try_get<Docked>(requester);
    if (docked == nullptr || !registry.valid(docked->station)) {
        return entt::null;
    }
    return docked->station;
}

void ProcessBuyRequests(entt::registry& registry) {
    std::vector<entt::entity> consumed;
    for (auto [self, request] : registry.view<BuyItemRequest>().each()) {
        consumed.push_back(self);

        const entt::entity station = DockedStation(registry, self);
        Wallet* wallet = registry.try_get<Wallet>(self);
        CargoHold* buyerCargo = registry.try_get<CargoHold>(self);
        CargoHold* stationCargo =
            station == entt::null ? nullptr : registry.try_get<CargoHold>(station);
        if (wallet == nullptr || buyerCargo == nullptr || stationCargo == nullptr ||
            wallet->credits < request.cost) {
            continue;
        }

        const auto held =
            std::find(stationCargo->modules.begin(), stationCargo->modules.end(), request.module);
        if (held == stationCargo->modules.end()) {
            continue;
        }

        wallet->credits -= request.cost;
        stationCargo->modules.erase(held);
        buyerCargo->modules.push_back(request.module);
    }
    for (const entt::entity self : consumed) {
        registry.remove<BuyItemRequest>(self);
    }
}

void ProcessSellRequests(entt::registry& registry) {
    std::vector<entt::entity> consumed;
    for (auto [self, request] : registry.view<SellItemRequest>().each()) {
        consumed.push_back(self);

        const entt::entity station = DockedStation(registry, self);
        Wallet* wallet = registry.try_get<Wallet>(self);
        CargoHold* sellerCargo = registry.try_get<CargoHold>(self);
        CargoHold* stationCargo =
            station == entt::null ? nullptr : registry.try_get<CargoHold>(station);
        if (wallet == nullptr || sellerCargo == nullptr || stationCargo == nullptr) {
            continue;
        }

        const auto held =
            std::find(sellerCargo->modules.begin(), sellerCargo->modules.end(), request.module);
        if (held == sellerCargo->modules.end()) {
            continue;
        }

        sellerCargo->modules.erase(held);
        stationCargo->modules.push_back(request.module);
        wallet->credits += request.value;
    }
    for (const entt::entity self : consumed) {
        registry.remove<SellItemRequest>(self);
    }
}

void ProcessRepairRequests(entt::registry& registry) {
    std::vector<entt::entity> consumed;
    for (auto [self, request] : registry.view<RepairRequest>().each()) {
        consumed.push_back(self);

        const entt::entity station = DockedStation(registry, self);
        Wallet* wallet = registry.try_get<Wallet>(self);
        const Rig* rig = registry.try_get<Rig>(self);
        if (station == entt::null || wallet == nullptr || rig == nullptr) {
            continue;
        }

        const int spend = static_cast<int>(
            std::round(request.fraction * static_cast<float>(request.costForFullRepair)));
        if (wallet->credits < spend) {
            continue;
        }
        wallet->credits -= spend;

        for (const entt::entity hardpoint : rig->children) {
            // architecture.md 12.30.7's Destroyed sweep: repair must not heal a permanently dead
            // hardpoint -- features.md 3.9's colour-is-condition schematic would draw it green.
            if (registry.all_of<Destroyed>(hardpoint)) {
                continue;
            }
            Health* health = registry.try_get<Health>(hardpoint);
            if (health == nullptr) {
                continue;
            }
            const float missing = health->max - health->current;
            health->current = std::min(health->max, health->current + request.fraction * missing);
        }
    }
    for (const entt::entity self : consumed) {
        registry.remove<RepairRequest>(self);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    ProcessBuyRequests(registry);
    ProcessSellRequests(registry);
    ProcessRepairRequests(registry);
}

}  // namespace sr::space::station_services_system
