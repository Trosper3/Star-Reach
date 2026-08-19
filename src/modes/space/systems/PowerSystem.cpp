#include "modes/space/systems/PowerSystem.h"

#include <algorithm>
#include <array>
#include <vector>

#include "shared/components/Power.h"
#include "shared/components/Rig.h"

namespace sr::space::power_system {
namespace {

constexpr size_t kCategoryCount = 4;  // PowerCategory::{Weapons, Shields, Engines, Facilities}
using LevelArray = std::array<PowerLevel, kCategoryCount>;

size_t CategoryIndex(PowerCategory category) {
    return static_cast<size_t>(category);
}

PowerLevel CommandedLevel(const PowerAllocation* allocation, PowerCategory category) {
    if (allocation == nullptr) {
        return PowerLevel::Normal;
    }
    switch (category) {
        case PowerCategory::Weapons: return allocation->weapons;
        case PowerCategory::Shields: return allocation->shields;
        case PowerCategory::Engines: return allocation->engines;
        case PowerCategory::Facilities: return allocation->facilities;
    }
    return PowerLevel::Normal;
}

const PowerLevelStats& StatsFor(const PowerLevels& levels, PowerLevel level) {
    switch (level) {
        case PowerLevel::Offline: return levels.offline;
        case PowerLevel::Reduced: return levels.reduced;
        case PowerLevel::Normal: return PowerLevels::normal;
        case PowerLevel::Boosted: return levels.boosted;
    }
    return PowerLevels::normal;
}

struct LoadEntry {
    entt::entity hardpoint;
    PowerCategory category;
    float authoredDraw;
    const PowerLevels* levels;
};

void ClearShed(entt::registry& registry, const Rig& rig) {
    for (const entt::entity child : rig.children) {
        registry.remove<PowerShed>(child);
    }
}

// Every living hardpoint's PowerSource/PowerLoad on `rig`. `generation` is written directly;
// loads are returned for the caller to resolve against whatever level each category ends up at.
std::vector<LoadEntry> BuildLoads(entt::registry& registry, const Rig& rig, float& generation) {
    std::vector<LoadEntry> loads;
    generation = 0.0f;
    for (const entt::entity child : rig.children) {
        if (registry.all_of<Destroyed>(child)) {
            continue;
        }
        if (const auto* source = registry.try_get<PowerSource>(child)) {
            generation += source->generation;
        }
        if (const auto* load = registry.try_get<PowerLoad>(child)) {
            loads.push_back({child, load->category, load->authoredDraw, &load->levels});
        }
    }
    return loads;
}

// Total draw across every load, each scaled by its own category's level in `effectiveLevel`.
float TotalDraw(const std::vector<LoadEntry>& loads, const LevelArray& effectiveLevel) {
    float draw = 0.0f;
    for (const LoadEntry& entry : loads) {
        const PowerLevelStats& stats =
            StatsFor(*entry.levels, effectiveLevel[CategoryIndex(entry.category)]);
        draw += entry.authoredDraw * stats.drawMultiplier;
    }
    return draw;
}

// Boost simply does not engage without headroom (features.md 2.9) -- refuse every commanded
// Boosted category (clamp back to Normal), rather than shedding something else to fund it.
// Returns the recomputed draw only if it actually refused something; otherwise `draw` unchanged.
float RefuseUnfundedBoosts(const std::vector<LoadEntry>& loads, LevelArray& effectiveLevel,
                           float draw) {
    bool refusedAny = false;
    for (PowerLevel& level : effectiveLevel) {
        if (level == PowerLevel::Boosted) {
            level = PowerLevel::Normal;
            refusedAny = true;
        }
    }
    return refusedAny ? TotalDraw(loads, effectiveLevel) : draw;
}

// Still overdrawn even with every boost refused -- architecture.md 12.16 item 18's other case,
// generation genuinely short of what even Normal costs (a dead power cell), kept as a separate
// path from the allocation-overcommit refusal above. Sheds whole categories, in `order`, to
// Offline until draw fits generation or everything is -- the same shape PowerLoad::priority used
// to drive before shed order became player-configurable instead of fixed by ModuleKind.
float ShedByPriority(entt::registry& registry, const std::vector<LoadEntry>& loads,
                     LevelArray& effectiveLevel,
                     const std::array<PowerCategory, kCategoryCount>& order, float generation,
                     float draw) {
    for (const PowerCategory category : order) {
        if (draw <= generation) {
            break;
        }
        const size_t index = CategoryIndex(category);
        if (effectiveLevel[index] == PowerLevel::Offline) {
            continue;
        }
        effectiveLevel[index] = PowerLevel::Offline;
        for (const LoadEntry& entry : loads) {
            if (entry.category == category) {
                registry.emplace_or_replace<PowerShed>(entry.hardpoint);
            }
        }
        draw = TotalDraw(loads, effectiveLevel);
    }
    return draw;
}

// The per-category readout WeaponSystem/PhysicsSystem read: the strongest authored
// effectMultiplier among a category's own living loads at its resolved level, times a final
// throttle factor for the case even a full shed could not fully fund (a genuine generation
// shortfall). A category with no living load stays at the harmless default of 1.0 -- nothing
// reads it either.
void WriteBudgetCategories(PowerBudget& budget, const std::vector<LoadEntry>& loads,
                           const LevelArray& effectiveLevel, float scaleFactor) {
    std::array<float, kCategoryCount> effect = {1.0f, 1.0f, 1.0f, 1.0f};
    std::array<bool, kCategoryCount> present = {false, false, false, false};
    for (const LoadEntry& entry : loads) {
        const size_t index = CategoryIndex(entry.category);
        const PowerLevelStats& stats = StatsFor(*entry.levels, effectiveLevel[index]);
        effect[index] = present[index] ? std::max(effect[index], stats.effectMultiplier)
                                       : stats.effectMultiplier;
        present[index] = true;
    }
    budget.weapons = effect[CategoryIndex(PowerCategory::Weapons)] * scaleFactor;
    budget.shields = effect[CategoryIndex(PowerCategory::Shields)] * scaleFactor;
    budget.engines = effect[CategoryIndex(PowerCategory::Engines)] * scaleFactor;
    budget.facilities = effect[CategoryIndex(PowerCategory::Facilities)] * scaleFactor;
}

void ResolveRig(entt::registry& registry, entt::entity root, const Rig& rig, PowerBudget& budget) {
    float generation = 0.0f;
    const std::vector<LoadEntry> loads = BuildLoads(registry, rig, generation);
    ClearShed(registry, rig);
    budget.generation = generation;

    const auto* allocation = registry.try_get<PowerAllocation>(root);
    LevelArray effectiveLevel = {
        CommandedLevel(allocation, PowerCategory::Weapons),
        CommandedLevel(allocation, PowerCategory::Shields),
        CommandedLevel(allocation, PowerCategory::Engines),
        CommandedLevel(allocation, PowerCategory::Facilities),
    };

    float draw = TotalDraw(loads, effectiveLevel);
    if (draw > generation) {
        draw = RefuseUnfundedBoosts(loads, effectiveLevel, draw);
    }

    float scaleFactor = 1.0f;
    if (draw > generation) {
        const auto* priorityList = registry.try_get<PowerPriorityList>(root);
        const auto order =
            priorityList != nullptr ? priorityList->order : PowerPriorityList{}.order;
        draw = ShedByPriority(registry, loads, effectiveLevel, order, generation, draw);
        // Whatever generation still cannot cover even with every category Offline throttles
        // proportionally rather than going negative -- the same clamp the old flat satisfaction
        // used.
        scaleFactor = draw > 0.0f ? std::clamp(generation / draw, 0.0f, 1.0f) : 1.0f;
    }

    budget.draw = draw;
    WriteBudgetCategories(budget, loads, effectiveLevel, scaleFactor);
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    for (auto [root, rig, budget] : registry.view<Rig, PowerBudget>().each()) {
        ResolveRig(registry, root, rig, budget);
    }
}

}  // namespace sr::space::power_system
