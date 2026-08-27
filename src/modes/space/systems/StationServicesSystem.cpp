#include "modes/space/systems/StationServicesSystem.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/economy/Pricing.h"
#include "core/registries/ContentLibrary.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
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

// The unit mass of an existing (kind, id) stack already held by `rigRoot`, or a negative value if
// no such stack exists there. A transfer moves an already-resolved stack, never a fresh deposit,
// so this reads mass off the source rather than re-resolving through ContentLibrary the way
// Buy/Sell must (shared/ may not include core/ -- architecture.md 2.3).
float ResolveUnitMass(const entt::registry& registry, entt::entity rigRoot, ItemKind kind,
                      const std::string& id) {
    for (const ItemStack& stack : cargo_view::Merged(registry, rigRoot)) {
        if (stack.kind == kind && stack.id == id) {
            return stack.unitMass;
        }
    }
    return -1.0f;
}

// architecture.md 12.30.3's ownership answer: a transfer within one owner is free; the station's
// CargoHold has exactly one owner, and Deposit/Withdraw are offered only when that owner is the
// requester. Buy/Sell (crossing an ownership boundary, at a price) is the Market half, P6-08.
void ProcessTransferRequests(entt::registry& registry) {
    std::vector<entt::entity> consumed;
    for (auto [self, request] : registry.view<TransferItemRequest>().each()) {
        consumed.push_back(self);

        const entt::entity station = DockedStation(registry, self);
        const FactionRef* stationFaction =
            station == entt::null ? nullptr : registry.try_get<FactionRef>(station);
        const FactionRef* requesterFaction = registry.try_get<FactionRef>(self);
        if (station == entt::null || stationFaction == nullptr || requesterFaction == nullptr ||
            stationFaction->id != requesterFaction->id || request.quantity <= 0) {
            continue;
        }

        const entt::entity source = request.toStation ? self : station;
        const entt::entity destination = request.toStation ? station : self;

        const float unitMass = ResolveUnitMass(registry, source, request.kind, request.id);
        if (unitMass < 0.0f) {
            continue;  // The source does not hold it -- refused whole, nothing moves.
        }
        if (!cargo_view::Withdraw(registry, source, request.kind, request.id, request.quantity)) {
            continue;
        }

        const ItemStack stack{request.kind, request.id, request.quantity, unitMass};
        const cargo_view::DepositResult depositResult =
            request.targetHold != entt::null
                ? cargo_view::Deposit(registry, destination, request.targetHold, stack)
                : cargo_view::Deposit(registry, destination, stack);
        if (depositResult != cargo_view::DepositResult::Deposited) {
            // The destination has no room -- undo the withdrawal rather than lose the stack.
            // architecture.md 12.30.3: "every transfer checks the destination and is refused
            // whole, never partially applied."
            cargo_view::Deposit(registry, source, stack);
            continue;
        }
    }
    for (const entt::entity self : consumed) {
        registry.remove<TransferItemRequest>(self);
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

// The facility's authored heal rate. FacilityRef itself carries only kind/grade (architecture.md
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

// architecture.md 12.30.4: a rig you own, present here -- your own vessel, or the station you are
// standing in when its FactionRef is yours (12.30.3's ownership test, "a station with a repair
// bay repairs itself"). Nothing else is a valid subject.
bool IsValidRepairSubject(const entt::registry& registry, entt::entity self, entt::entity station,
                          entt::entity subject) {
    if (subject == self) {
        return true;
    }
    if (subject != station) {
        return false;
    }
    const FactionRef* stationFaction = registry.try_get<FactionRef>(station);
    const FactionRef* selfFaction = registry.try_get<FactionRef>(self);
    return stationFaction != nullptr && selfFaction != nullptr &&
           stationFaction->id == selfFaction->id;
}

// Every living, Health-bearing hardpoint the order applies to: `hardpoint` itself if it names one
// (and belongs to `subject` and is not Destroyed), else every living hardpoint of `subject`'s
// Rig -- entt::null means "Repair All." Empty if `subject` has no Rig, or a named `hardpoint`
// does not belong to it or is gone.
std::vector<entt::entity> RepairableHardpoints(const entt::registry& registry, entt::entity subject,
                                               entt::entity hardpoint) {
    std::vector<entt::entity> result;
    const Rig* rig = registry.try_get<Rig>(subject);
    if (rig == nullptr) {
        return result;
    }
    for (const entt::entity candidate : rig->children) {
        if (hardpoint != entt::null && candidate != hardpoint) {
            continue;
        }
        // architecture.md 12.30.7's Destroyed sweep: repair must not heal a permanently dead
        // hardpoint -- features.md 3.9's colour-is-condition schematic would draw it green.
        if (registry.all_of<Destroyed>(candidate) || !registry.all_of<Health>(candidate)) {
            continue;
        }
        result.push_back(candidate);
    }
    return result;
}

// Bills `self` for `totalHp` of repair at `facility`'s grade-based rate, preferring self's own
// Wallet, else the requester's FactionRef stock against ctx.economy. Returns false, leaving the
// remainder untouched, when nothing affordable exists to pay from -- the caller stalls rather
// than drops the order.
//
// The fractional remainder is carried on `self`'s own RepairBilling (shared/components/
// StationServices.h), get_or_emplace'd here rather than threaded in from RepairOrder -- issue
// #268: a RepairOrder is destroyed and recreated every time the player toggles repair off and
// back on (modes/space/ui/RepairScreen.cpp's ToggleOrder), and a remainder living on it would
// reset to zero on every restart, rounding every tick's charge down to zero forever regardless of
// Wallet balance. RepairBilling outlives that toggle entirely, so restarting cannot launder away
// an already-accrued fraction of a credit.
bool ChargeRepairCost(const SystemContext& ctx, entt::entity self, entt::entity facility,
                      float totalHp) {
    entt::registry& registry = ctx.Registry();
    const FacilityRef* facilityRef = registry.try_get<FacilityRef>(facility);
    const int grade = facilityRef != nullptr ? facilityRef->grade : 1;
    const int costPerHp = core::economy::RepairCostPerHp(grade);
    RepairBilling& billing = registry.get_or_emplace<RepairBilling>(self);
    const float owed = totalHp * static_cast<float>(costPerHp) + billing.creditRemainder;
    const int spend = static_cast<int>(std::floor(owed));

    // architecture.md 12.30.4: "Wallet on a rig that has one, ctx.economy otherwise" -- billed
    // against the REQUESTER (self), never the subject: an NPC always repairs itself (self ==
    // subject), but a player repairing their own station still pays from their own Wallet, not
    // one the station does not carry.
    bool afforded = false;
    if (Wallet* wallet = registry.try_get<Wallet>(self); wallet != nullptr) {
        afforded = wallet->credits >= spend;
        if (afforded) {
            wallet->credits -= spend;
        }
    } else if (ctx.economy != nullptr) {
        if (const FactionRef* faction = registry.try_get<FactionRef>(self); faction != nullptr) {
            afforded = ctx.economy->Spend(faction->id, spend);
        }
    }
    if (afforded) {
        billing.creditRemainder = owed - static_cast<float>(spend);
    }
    return afforded;
}

void ProcessRepairOrders(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> toRemove;

    for (auto [self, order] : registry.view<RepairOrder>().each()) {
        const entt::entity station = DockedStation(registry, self);
        if (station == entt::null) {
            toRemove.push_back(self);  // Undocked -- the order is dropped, never resumed.
            continue;
        }
        if (!IsValidRepairSubject(registry, self, station, order.subject)) {
            toRemove.push_back(self);  // Invalidated.
            continue;
        }
        const entt::entity facility = DockedRepairFacility(registry, station);
        if (facility == entt::null) {
            toRemove.push_back(self);  // The Repair hardpoint is gone -- stops that tick.
            continue;
        }

        const std::vector<entt::entity> hardpoints =
            RepairableHardpoints(registry, order.subject, order.hardpoint);
        if (hardpoints.empty()) {
            toRemove.push_back(self);  // Nothing left this order could ever apply to.
            continue;
        }

        const float rateHp = RepairFacilityRate(registry, ctx.content, facility) * ctx.dt;

        struct Pending {
            entt::entity hardpoint;
            float amount;
        };
        std::vector<Pending> pending;
        float totalHp = 0.0f;
        bool allAtTarget = true;
        for (const entt::entity hardpoint : hardpoints) {
            const Health& health = registry.get<Health>(hardpoint);
            const float target = order.targetFraction * health.max;
            const float missing = target - health.current;
            if (missing > 0.0f) {
                allAtTarget = false;
                const float amount = std::min(missing, rateHp);
                if (amount > 0.0f) {
                    pending.push_back({hardpoint, amount});
                    totalHp += amount;
                }
            }
        }

        if (allAtTarget) {
            toRemove.push_back(self);  // Target reached -- order completes.
            continue;
        }
        if (totalHp <= 0.0f) {
            continue;  // The facility's rate delivers nothing this tick -- stalls, not gone.
        }

        if (!ChargeRepairCost(ctx, self, facility, totalHp)) {
            continue;  // Stalls where the money ran out -- nothing owed, nothing refunded.
        }

        for (const Pending& p : pending) {
            registry.get<Health>(p.hardpoint).current += p.amount;
        }
    }

    for (const entt::entity self : toRemove) {
        registry.remove<RepairOrder>(self);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    ProcessBuyRequests(registry, ctx.content);
    ProcessSellRequests(registry, ctx.content);
    ProcessRepairOrders(ctx);
    ProcessTransferRequests(registry);
}

}  // namespace sr::space::station_services_system
