#include <cstdio>
#include <cstdlib>
#include <vector>

#include "economy_sim/EconomyModel.h"

// tools/economy_sim -- see EconomyModel.h for the model itself. Prints the derived cost/time
// curve across the seven-tier grade ladder for one or more candidate quantity-per-grade values,
// so features.md 2.10's "cannot be read by inspection" three-knob curve is read from a real run
// instead of an estimate. features.md 9 names this tool as what settles manufacturing/royalty
// pacing; architecture.md 13.5 group 2c promotes it from deferred to a content-authoring
// prerequisite.
//
// Usage: economy_sim [quantityPerGrade ...]
//   No arguments: sweeps the candidate values under discussion (1.0, 1.5, 2.0, 3.0).
//   One or more arguments: prints the curve for exactly those quantity-per-grade values instead.

namespace {

void PrintCurve(double quantityPerGrade) {
    sr::economy_sim::CostModel model;
    model.quantityPerGrade = quantityPerGrade;

    std::printf("\nQuantity-per-grade = %.2f\n", quantityPerGrade);
    std::printf("%-10s %10s %10s %12s %10s %12s\n", "Grade", "Material", "Module", "CombatVal",
                "Cost/Val", "MatTime(s)");

    const double commonModule = sr::economy_sim::ModuleBaseValue(sr::economy_sim::Grade::Common, model);
    const double commonValue = sr::economy_sim::QualityBandUpper(sr::economy_sim::Grade::Common);

    for (int i = 0; i < sr::economy_sim::kGradeCount; ++i) {
        const auto grade = static_cast<sr::economy_sim::Grade>(i);
        const double materialCost = sr::economy_sim::MaterialBaseValue(grade, model);
        const double moduleCost = sr::economy_sim::ModuleBaseValue(grade, model);
        const double combatValue = sr::economy_sim::QualityBandUpper(grade);
        const double costRatio = moduleCost / commonModule;
        const double valueRatio = combatValue / commonValue;
        const double costPerValue = costRatio / valueRatio;
        const double matTime = sr::economy_sim::ManufacturingTimeSeconds(grade, false);

        std::printf("%-10s %10.1f %10.1f %12.2f %10.2f %12.1f\n", sr::economy_sim::GradeName(grade),
                    materialCost, moduleCost, combatValue, costPerValue, matTime);
    }

    const double mythicModule = sr::economy_sim::ModuleBaseValue(sr::economy_sim::Grade::Mythic, model);
    std::printf("Common -> Mythic module cost multiplier: %.0fx\n", mythicModule / commonModule);
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<double> candidates = {1.0, 1.5, 2.0, 3.0};

    if (argc > 1) {
        candidates.clear();
        for (int i = 1; i < argc; ++i) {
            candidates.push_back(std::atof(argv[i]));
        }
    }

    std::printf("StarReach economy_sim -- features.md 2.10's three-knob manufacturing cost curve\n");
    std::printf(
        "(Cost/Val above 1.0 means manufacturing cost outpaces combat-value gain -- features.md\n"
        " 2.4's hard constraint. A number in the thousands means the curve has rebuilt the\n"
        " scarcity ladder on the cost side, which 2.10 flags as its own failure mode.)\n");

    for (double q : candidates) {
        PrintCurve(q);
    }

    return 0;
}
