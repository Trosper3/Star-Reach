#include "modes/space/systems/PowerSystem.h"

#include <algorithm>
#include <vector>

#include "shared/components/Power.h"
#include "shared/components/Rig.h"

namespace sr::space::power_system {
namespace {

// Above 100% draw the rig throttles proportionally; past this band it starts shedding
// low-priority loads instead of throttling everything into uselessness. Matches the ratio
// StarReach2's RecalculatePowerBudget used for the same throttle-then-shed split.
constexpr float kThrottleBandRatio = 1.25f;

struct LoadEntry {
    entt::entity hardpoint;
    float draw;
    int priority;
};

void ClearShed(entt::registry& registry, const Rig& rig) {
    for (const entt::entity child : rig.children) {
        registry.remove<PowerShed>(child);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();

    for (auto [root, rig, budget] : registry.view<Rig, PowerBudget>().each()) {
        float generation = 0.0f;
        std::vector<LoadEntry> loads;
        for (const entt::entity child : rig.children) {
            if (registry.all_of<Destroyed>(child)) {
                continue;
            }
            if (const auto* source = registry.try_get<PowerSource>(child)) {
                generation += source->generation;
            }
            if (const auto* load = registry.try_get<PowerLoad>(child)) {
                loads.push_back({child, load->draw, load->priority});
            }
        }

        float draw = 0.0f;
        for (const LoadEntry& entry : loads) {
            draw += entry.draw;
        }

        budget.generation = generation;
        budget.draw = draw;
        ClearShed(registry, rig);

        if (draw <= generation) {
            budget.satisfaction = 1.0f;
            continue;
        }

        if (generation > 0.0f && draw / generation <= kThrottleBandRatio) {
            budget.satisfaction = std::clamp(generation / draw, 0.0f, 1.0f);
            continue;
        }

        // Severe overdraw: shed whole hardpoints, lowest priority first, until what remains fits
        // the throttle band -- see Power.h's PowerLoad::priority doc for the shedding order.
        std::stable_sort(loads.begin(), loads.end(), [](const LoadEntry& a, const LoadEntry& b) {
            return a.priority < b.priority;
        });

        float remaining = draw;
        for (const LoadEntry& entry : loads) {
            if (remaining <= generation * kThrottleBandRatio) {
                break;
            }
            registry.emplace_or_replace<PowerShed>(entry.hardpoint);
            remaining -= entry.draw;
        }

        budget.satisfaction =
            remaining > 0.0f ? std::clamp(generation / remaining, 0.0f, 1.0f) : 1.0f;
    }
}

}  // namespace sr::space::power_system
