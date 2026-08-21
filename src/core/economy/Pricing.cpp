#include "core/economy/Pricing.h"

#include <algorithm>

namespace sr::core::economy {
namespace {

// Placeholder base rate -- see Pricing.h's comment on why this cannot yet derive from a shell's
// recipe/Inert attribute. Not tuned.
constexpr int kBaseCostPerHp = 2;

}  // namespace

int RepairCostPerHp(int facilityGrade) {
    const int grade = std::max(facilityGrade, 1);
    return std::max(1, kBaseCostPerHp / grade);
}

}  // namespace sr::core::economy
