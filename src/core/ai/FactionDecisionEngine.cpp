#include "core/ai/FactionDecisionEngine.h"

#include <algorithm>

namespace sr::core::ai {
namespace {

constexpr float kMaterialSecurityRaidThreshold = 30.0f;
constexpr float kRaidDispatchChance = 0.45f;

}  // namespace

FacetProfile ComputeFacets(const economy::FactionEconomy& economy, const FactionId& faction) {
    FacetProfile facets;
    const int stock = economy.Stock(faction);
    const float ratio = static_cast<float>(stock) / static_cast<float>(kReferenceStock);
    facets.materialSecurity = std::clamp(ratio * 100.0f, 0.0f, 100.0f);
    return facets;
}

float RaidDispatchChance(const FacetProfile& facets) {
    return facets.materialSecurity < kMaterialSecurityRaidThreshold ? kRaidDispatchChance : 0.0f;
}

std::optional<RaidDispatchDirective> EvaluateRaidDispatch(const FactionId& faction,
                                                          const FacetProfile& facets, float roll) {
    const float chance = RaidDispatchChance(facets);
    if (chance <= 0.0f || roll >= chance) {
        return std::nullopt;
    }
    return RaidDispatchDirective{faction};
}

std::optional<ColonizeDirective> EvaluateColonization(const FactionId& faction,
                                                      const economy::FactionEconomy& economy,
                                                      const diplomacy::Territory& territory,
                                                      const std::string& candidateSystemId) {
    if (!territory.Owner(candidateSystemId).empty()) {
        return std::nullopt;  // Already claimed, by anyone.
    }
    if (economy.Stock(faction) < kReferenceStock) {
        return std::nullopt;
    }
    return ColonizeDirective{faction, candidateSystemId};
}

}  // namespace sr::core::ai
