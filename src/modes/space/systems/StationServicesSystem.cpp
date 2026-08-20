#include "modes/space/systems/StationServicesSystem.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/registries/ContentLibrary.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/components/StationServices.h"
#include "shared/rig/CargoView.h"

namespace sr::space::station_services_system {
namespace {

entt::entity DockedStation(const entt::registry& registry, entt::entity requester) {
    const Docked* docked = registry.try_get<Docked>(requester);
    if (docked == nullptr || !registry.valid(docked->station)) {
        return entt::null;
    }
    return docked->station;
}

void ProcessBuyRequests(entt::registry& registry, const core::ContentLibrary& content) {
    std::vector<entt::entity> consumed;
    for (auto [self, request] : registry.view<BuyItemRequest>().each()) {
        consumed.push_back(self);

        const entt::entity station = DockedStation(registry, self);
        Wallet* wallet = registry.try_get<Wallet>(self);
        const ModuleDef* module = content.FindModule(request.module);
        if (station == entt::null || wallet == nullptr || module == nullptr ||
            wallet->credits < request.cost) {
            continue;
        }

        const std::string id = request.module.str();
        if (!cargo_view::Withdraw(registry, station, ItemKind::Module, id, 1)) {
            continue;  // The station does not stock it.
        }

        const ItemStack stack{ItemKind::Module, id, 1, module->mass};
        if (cargo_view::Deposit(registry, self, stack) != cargo_view::DepositResult::Deposited) {
            // The buyer has nowhere to put it -- undo the withdrawal rather than lose the module.
            cargo_view::Deposit(registry, station, stack);
            continue;
        }

        wallet->credits -= request.cost;
    }
    for (const entt::entity self : consumed) {
        registry.remove<BuyItemRequest>(self);
    }
}

void ProcessSellRequests(entt::registry& registry, const core::ContentLibrary& content) {
    std::vector<entt::entity> consumed;
    for (auto [self, request] : registry.view<SellItemRequest>().each()) {
        consumed.push_back(self);

        const entt::entity station = DockedStation(registry, self);
        Wallet* wallet = registry.try_get<Wallet>(self);
        const ModuleDef* module = content.FindModule(request.module);
        if (station == entt::null || wallet == nullptr || module == nullptr) {
            continue;
        }

        const std::string id = request.module.str();
        if (!cargo_view::Withdraw(registry, self, ItemKind::Module, id, 1)) {
            continue;  // The seller does not hold it.
        }

        const ItemStack stack{ItemKind::Module, id, 1, module->mass};
        if (cargo_view::Deposit(registry, station, stack) != cargo_view::DepositResult::Deposited) {
            // The station has nowhere to put it -- undo the withdrawal rather than lose the module.
            cargo_view::Deposit(registry, self, stack);
            continue;
        }

        wallet->credits += request.value;
    }
    for (const entt::entity self : consumed) {
        registry.remove<SellItemRequest>(self);
    }
}

// The docked station's living Repair-kind hardpoint, or entt::null if there isn't one.
// architecture.md 13.3 finding I: the paid repair path used DockedStation only as a docked-ness
// check and never actually looked for a Repair facility, which made the free heal in
// DockingSystem the only thing gating repair at all. "Living" excludes Destroyed, the same rule
// EngineerSystem's DockedEngineeringLevel and RefactorSystem's DockedAtEngineeringFacility use.
entt::entity DockedRepairFacility(const entt::registry& registry, entt::entity station) {
    const Rig* stationRig = registry.try_get<Rig>(station);
    if (stationRig == nullptr) {
        return entt::null;
    }
    for (const entt::entity hardpoint : stationRig->children) {
        if (registry.all_of<Destroyed>(hardpoint)) {
            continue;
        }
        const FacilityRef* facility = registry.try_get<FacilityRef>(hardpoint);
        if (facility != nullptr && facility->kind == FacilityKind::Repair) {
            return hardpoint;
        }
    }
    return entt::null;
}

// The facility's authored heal rate. FacilityRef itself carries only kind/level (architecture.md
// 13.3 finding K) -- never a field's authored value -- so the rate has to come from the same
// content lookup RefactorSystem uses to resolve a hardpoint's ModuleDef: MountedModules names
// which module(s) RigFactory attached here, and FacilityStats::ratePerSecond (parsed, merged by
// EngineerSystem, read by nothing until now) lives on that ModuleDef.
//
// Multiplied by the STATION's own crew, not the requester's -- features.md 2.7's Repair role
// boosts the facility a Repair officer is stationed aboard, not whoever is being repaired.
// CrewRepairBonus is a rig-root aggregate (shared/rig/ModuleAttachment.cpp's RecomputeRigTotals,
// architecture.md 12.14 item 16's "Repair crew role's only named consumer"), read here via the
// facility hardpoint's own ParentRig rather than passed in, since the station root is otherwise
// only known one call up (DockedRepairFacility already resolved it against `station`).
float RepairFacilityRate(const entt::registry& registry, const core::ContentLibrary& content,
                         entt::entity facilityHardpoint) {
    const MountedModules* mounted = registry.try_get<MountedModules>(facilityHardpoint);
    if (mounted == nullptr) {
        return 0.0f;
    }
    float rate = 0.0f;
    for (const ModuleId& id : mounted->ids) {
        const ModuleDef* module = content.FindModule(id);
        if (module != nullptr && module->facility.kind == FacilityKind::Repair) {
            rate = module->facility.ratePerSecond;
            break;
        }
    }
    if (rate <= 0.0f) {
        return 0.0f;
    }
    if (const auto* parent = registry.try_get<ParentRig>(facilityHardpoint)) {
        if (const auto* bonus = registry.try_get<CrewRepairBonus>(parent->root)) {
            rate *= (1.0f + bonus->value);
        }
    }
    return rate;
}

void ProcessRepairRequests(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> consumed;
    for (auto [self, request] : registry.view<RepairRequest>().each()) {
        consumed.push_back(self);

        const entt::entity station = DockedStation(registry, self);
        Wallet* wallet = registry.try_get<Wallet>(self);
        const Rig* rig = registry.try_get<Rig>(self);
        const entt::entity facility =
            station == entt::null ? entt::null : DockedRepairFacility(registry, station);
        if (station == entt::null || wallet == nullptr || rig == nullptr ||
            facility == entt::null) {
            continue;
        }

        // The facility's rate caps how much of the requested fraction this tick can actually
        // deliver -- a slow bay cannot instantly repair to the fraction paid for. Billing scales
        // with the same capped fraction, never the requested one, so a capped tick never charges
        // for hull it did not restore.
        const float rate = RepairFacilityRate(registry, ctx.content, facility);
        const float achievedFraction = std::min(request.fraction, rate * ctx.dt);

        const int spend = static_cast<int>(
            std::round(achievedFraction * static_cast<float>(request.costForFullRepair)));
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
            health->current = std::min(health->max, health->current + achievedFraction * missing);
        }
    }
    for (const entt::entity self : consumed) {
        registry.remove<RepairRequest>(self);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    ProcessBuyRequests(registry, ctx.content);
    ProcessSellRequests(registry, ctx.content);
    ProcessRepairRequests(ctx);
}

}  // namespace sr::space::station_services_system
