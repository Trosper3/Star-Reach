#include "economy_sim/EconomyModel.h"

#include <array>
#include <cmath>

namespace sr::economy_sim {

namespace {

int Index(Grade grade) {
    return static_cast<int>(grade);
}

// features.md 2.7's quality band upper bounds, Common..Mythic.
constexpr std::array<double, kGradeCount> kQualityUpper = {1.10, 1.30, 1.55, 1.85, 2.50, 3.50, 5.00};

}  // namespace

const char* GradeName(Grade grade) {
    switch (grade) {
        case Grade::Common:
            return "Common";
        case Grade::Uncommon:
            return "Uncommon";
        case Grade::Unique:
            return "Unique";
        case Grade::Rare:
            return "Rare";
        case Grade::Epic:
            return "Epic";
        case Grade::Legendary:
            return "Legendary";
        case Grade::Mythic:
            return "Mythic";
    }
    return "Unknown";
}

int SlotBreadth(Grade grade) {
    return 2 + Index(grade);
}

double QualityBandUpper(Grade grade) {
    return kQualityUpper[static_cast<std::size_t>(Index(grade))];
}

double MaterialBaseValue(Grade grade, const CostModel& model) {
    const double quantity = std::pow(model.quantityPerGrade, Index(grade));
    const double raw = static_cast<double>(SlotBreadth(grade)) * quantity * kElementBaseValue;
    return raw * (1.0 + model.marginRate);
}

double ModuleBaseValue(Grade grade, const CostModel& model) {
    const int inputIndex = Index(grade) > 0 ? Index(grade) - 1 : 0;
    const double inputCost = MaterialBaseValue(static_cast<Grade>(inputIndex), model);
    const double quantity = std::pow(model.quantityPerGrade, Index(grade));
    const double raw = static_cast<double>(SlotBreadth(grade)) * quantity * inputCost;
    return raw * (1.0 + model.marginRate);
}

double ManufacturingTimeSeconds(Grade grade, bool isModule) {
    const double base = isModule ? 10.0 : 5.0;
    return base * std::pow(2.0, Index(grade));
}

}  // namespace sr::economy_sim
