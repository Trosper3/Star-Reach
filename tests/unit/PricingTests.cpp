#include <catch2/catch_test_macros.hpp>

#include "core/economy/Pricing.h"

using sr::core::economy::RepairCostPerHp;

TEST_CASE("RepairCostPerHp is cheaper at a higher facility grade", "[pricing]") {
    const int grade1 = RepairCostPerHp(1);
    const int grade2 = RepairCostPerHp(2);
    CHECK(grade1 > 0);
    CHECK(grade2 <= grade1);
}

TEST_CASE("RepairCostPerHp never returns less than one credit per HP", "[pricing]") {
    CHECK(RepairCostPerHp(1) >= 1);
    CHECK(RepairCostPerHp(100) >= 1);
}

TEST_CASE("RepairCostPerHp clamps a misauthored grade of zero or below to 1", "[pricing]") {
    CHECK(RepairCostPerHp(0) == RepairCostPerHp(1));
    CHECK(RepairCostPerHp(-5) == RepairCostPerHp(1));
}
