#include "shared/rig/CargoView.h"

#include <algorithm>
#include <limits>

#include "shared/components/Identity.h"
#include "shared/components/Rig.h"

namespace sr::cargo_view {
namespace {

// Living cargo-bay hardpoints under rigRoot, in Rig::children order -- callers that care about
// placement order re-sort; Capacity/TotalMass/Merged don't care about order at all.
std::vector<entt::entity> LivingBays(const entt::registry& registry, entt::entity rigRoot) {
    std::vector<entt::entity> bays;
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return bays;
    }
    for (const entt::entity hardpoint : rig->children) {
        if (!registry.all_of<Destroyed>(hardpoint) && registry.all_of<CargoHold>(hardpoint)) {
            bays.push_back(hardpoint);
        }
    }
    return bays;
}

// Emptiest-first by fraction of slotCount used, ties broken by MountId -- never Rig::children
// order (architecture.md 12.23), so the same rig built with its mounts authored in a different
// order places identically.
void SortEmptiestFirst(const entt::registry& registry, std::vector<entt::entity>& bays) {
    std::sort(bays.begin(), bays.end(), [&](entt::entity a, entt::entity b) {
        const CargoHold& cargoA = registry.get<CargoHold>(a);
        const CargoHold& cargoB = registry.get<CargoHold>(b);
        const float fracA = cargoA.slotCount > 0
                                ? static_cast<float>(cargoA.stacks.size()) / cargoA.slotCount
                                : 1.0f;
        const float fracB = cargoB.slotCount > 0
                                ? static_cast<float>(cargoB.stacks.size()) / cargoB.slotCount
                                : 1.0f;
        if (fracA != fracB) {
            return fracA < fracB;
        }
        const MountRef* refA = registry.try_get<MountRef>(a);
        const MountRef* refB = registry.try_get<MountRef>(b);
        const std::string idA = refA != nullptr ? refA->id.str() : "";
        const std::string idB = refB != nullptr ? refB->id.str() : "";
        return idA < idB;
    });
}

// How many whole units of `unitMass` fit under one slot's capacity. int max (never the limiter)
// for a massless item rather than dividing by zero.
int UnitsPerSlot(float slotCapacity, float unitMass) {
    if (unitMass <= 0.0f) {
        return std::numeric_limits<int>::max();
    }
    return static_cast<int>(slotCapacity / unitMass);
}

struct TopUp {
    entt::entity hardpoint;
    std::size_t index;
    int amount;
};
struct NewSlot {
    entt::entity hardpoint;
    int amount;
};

// Deposit pass 1: top up an existing matching stack with room. Module stacks never merge
// (ItemStack's comment), so this is a no-op for ItemKind::Module. Returns true if a matching
// stack was seen at all -- even one already full -- for Deposit's HoldFull/NoFreeSlot call.
bool TopUpMatchingStacks(entt::registry& registry, const std::vector<entt::entity>& bays,
                         const ItemStack& stack, int& remaining, std::vector<TopUp>& topUps) {
    bool sawMatchingStack = false;
    if (stack.kind != ItemKind::Element) {
        return sawMatchingStack;
    }
    for (const entt::entity hardpoint : bays) {
        CargoHold& cargo = registry.get<CargoHold>(hardpoint);
        for (std::size_t i = 0; i < cargo.stacks.size() && remaining > 0; ++i) {
            const ItemStack& existing = cargo.stacks[i];
            if (existing.kind != ItemKind::Element || existing.id != stack.id) {
                continue;
            }
            sawMatchingStack = true;
            const float usedMass = static_cast<float>(existing.quantity) * existing.unitMass;
            const float roomMass = cargo.slotCapacity - usedMass;
            const int roomUnits =
                stack.unitMass > 0.0f ? static_cast<int>(roomMass / stack.unitMass) : remaining;
            const int take = std::max(0, std::min(remaining, roomUnits));
            if (take == 0) {
                continue;
            }
            topUps.push_back({hardpoint, i, take});
            remaining -= take;
        }
    }
    return sawMatchingStack;
}

// Deposit pass 2: place whatever remains into free slots, emptiest-bay-first (`bays` is
// pre-sorted), splitting across bays -- and, for a bulk deposit, across multiple slots within one
// bay -- as needed. Returns true if any free slot was seen at all, for the same classification.
bool FillFreeSlots(const entt::registry& registry, const std::vector<entt::entity>& bays,
                   const ItemStack& stack, int& remaining, std::vector<NewSlot>& newSlots) {
    bool sawFreeSlot = false;
    for (const entt::entity hardpoint : bays) {
        const CargoHold& cargo = registry.get<CargoHold>(hardpoint);
        int freeSlots = cargo.slotCount - static_cast<int>(cargo.stacks.size());
        while (remaining > 0 && freeSlots > 0) {
            sawFreeSlot = true;
            const int perSlot = UnitsPerSlot(cargo.slotCapacity, stack.unitMass);
            const int take = std::max(0, std::min(remaining, perSlot));
            if (take == 0) {
                break;  // Not even one unit fits this bay's slotCapacity -- try the next bay.
            }
            newSlots.push_back({hardpoint, take});
            remaining -= take;
            --freeSlots;
            if (stack.kind == ItemKind::Module) {
                break;  // A module stack is always exactly one unit; never split further.
            }
        }
    }
    return sawFreeSlot;
}

}  // namespace

float Capacity(const entt::registry& registry, entt::entity rigRoot) {
    float total = 0.0f;
    for (const entt::entity hardpoint : LivingBays(registry, rigRoot)) {
        const CargoHold& cargo = registry.get<CargoHold>(hardpoint);
        total += static_cast<float>(cargo.slotCount) * cargo.slotCapacity;
    }
    return total;
}

float TotalMass(const entt::registry& registry, entt::entity rigRoot) {
    float total = 0.0f;
    for (const entt::entity hardpoint : LivingBays(registry, rigRoot)) {
        for (const ItemStack& stack : registry.get<CargoHold>(hardpoint).stacks) {
            total += static_cast<float>(stack.quantity) * stack.unitMass;
        }
    }
    return total;
}

DepositResult Deposit(entt::registry& registry, entt::entity rigRoot, const ItemStack& stack) {
    if (stack.quantity <= 0) {
        return DepositResult::Deposited;
    }

    std::vector<entt::entity> bays = LivingBays(registry, rigRoot);
    SortEmptiestFirst(registry, bays);

    int remaining = stack.quantity;
    std::vector<TopUp> topUps;
    std::vector<NewSlot> newSlots;
    const bool sawMatchingStack = TopUpMatchingStacks(registry, bays, stack, remaining, topUps);
    const bool sawFreeSlot = FillFreeSlots(registry, bays, stack, remaining, newSlots);

    if (remaining > 0) {
        // Something existed to place against (a free slot or a matching stack) but still wasn't
        // enough -> HoldFull. Nothing at all existed for this item -> NoFreeSlot.
        return sawFreeSlot || sawMatchingStack ? DepositResult::HoldFull
                                               : DepositResult::NoFreeSlot;
    }

    for (const TopUp& t : topUps) {
        registry.get<CargoHold>(t.hardpoint).stacks[t.index].quantity += t.amount;
    }
    for (const NewSlot& n : newSlots) {
        registry.get<CargoHold>(n.hardpoint)
            .stacks.push_back(ItemStack{stack.kind, stack.id, n.amount, stack.unitMass});
    }
    return DepositResult::Deposited;
}

bool Withdraw(entt::registry& registry, entt::entity rigRoot, ItemKind kind, const std::string& id,
              int quantity) {
    if (quantity <= 0) {
        return true;
    }

    struct Match {
        entt::entity hardpoint;
        std::size_t index;
        float mass;
    };
    std::vector<Match> matches;
    for (const entt::entity hardpoint : LivingBays(registry, rigRoot)) {
        const CargoHold& cargo = registry.get<CargoHold>(hardpoint);
        for (std::size_t i = 0; i < cargo.stacks.size(); ++i) {
            const ItemStack& stack = cargo.stacks[i];
            if (stack.kind == kind && stack.id == id) {
                matches.push_back(
                    {hardpoint, i, static_cast<float>(stack.quantity) * stack.unitMass});
            }
        }
    }

    int available = 0;
    for (const Match& match : matches) {
        available += registry.get<CargoHold>(match.hardpoint).stacks[match.index].quantity;
    }
    if (available < quantity) {
        return false;  // Whole or nothing -- refuse before writing anything.
    }

    // Fullest matching slot first, so bays drift back toward even.
    std::sort(matches.begin(), matches.end(),
              [](const Match& a, const Match& b) { return a.mass > b.mass; });

    int remaining = quantity;
    for (const Match& match : matches) {
        if (remaining <= 0) {
            break;
        }
        ItemStack& stack = registry.get<CargoHold>(match.hardpoint).stacks[match.index];
        const int take = std::min(remaining, stack.quantity);
        stack.quantity -= take;
        remaining -= take;
    }

    for (const entt::entity hardpoint : LivingBays(registry, rigRoot)) {
        std::vector<ItemStack>& stacks = registry.get<CargoHold>(hardpoint).stacks;
        stacks.erase(std::remove_if(stacks.begin(), stacks.end(),
                                    [](const ItemStack& stack) { return stack.quantity <= 0; }),
                     stacks.end());
    }
    return true;
}

std::vector<ItemStack> Merged(const entt::registry& registry, entt::entity rigRoot) {
    std::vector<ItemStack> out;
    for (const entt::entity hardpoint : LivingBays(registry, rigRoot)) {
        const std::vector<ItemStack>& stacks = registry.get<CargoHold>(hardpoint).stacks;
        out.insert(out.end(), stacks.begin(), stacks.end());
    }
    return out;
}

}  // namespace sr::cargo_view
