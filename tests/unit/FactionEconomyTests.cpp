#include <catch2/catch_test_macros.hpp>

#include "core/economy/FactionEconomy.h"

using sr::FactionId;
using sr::core::economy::FactionEconomy;

TEST_CASE("FactionEconomy starts every faction at zero stock", "[economy]") {
    FactionEconomy economy;
    CHECK(economy.Stock(FactionId("aegis")) == 0);
}

TEST_CASE("FactionEconomy accumulates deposits", "[economy]") {
    FactionEconomy economy;
    economy.Deposit(FactionId("aegis"), 100);
    economy.Deposit(FactionId("aegis"), 50);
    CHECK(economy.Stock(FactionId("aegis")) == 150);
}

TEST_CASE("FactionEconomy ignores a non-positive deposit", "[economy]") {
    FactionEconomy economy;
    economy.Deposit(FactionId("aegis"), 0);
    economy.Deposit(FactionId("aegis"), -50);
    CHECK(economy.Stock(FactionId("aegis")) == 0);
}

TEST_CASE("FactionEconomy keeps each faction's stock independent", "[economy]") {
    FactionEconomy economy;
    economy.Deposit(FactionId("aegis"), 100);
    economy.Deposit(FactionId("reavers"), 40);
    CHECK(economy.Stock(FactionId("aegis")) == 100);
    CHECK(economy.Stock(FactionId("reavers")) == 40);
}

TEST_CASE("FactionEconomy spends when the faction can afford it", "[economy]") {
    FactionEconomy economy;
    economy.Deposit(FactionId("aegis"), 100);
    CHECK(economy.Spend(FactionId("aegis"), 60));
    CHECK(economy.Stock(FactionId("aegis")) == 40);
}

TEST_CASE("FactionEconomy refuses to spend more than the faction has, changing nothing",
          "[economy]") {
    FactionEconomy economy;
    economy.Deposit(FactionId("aegis"), 30);
    CHECK_FALSE(economy.Spend(FactionId("aegis"), 60));
    CHECK(economy.Stock(FactionId("aegis")) == 30);
}

TEST_CASE("FactionEconomy refuses to spend against a faction with no stock at all", "[economy]") {
    FactionEconomy economy;
    CHECK_FALSE(economy.Spend(FactionId("aegis"), 1));
}

TEST_CASE("FactionEconomy treats a non-positive spend as an automatic success", "[economy]") {
    FactionEconomy economy;
    CHECK(economy.Spend(FactionId("aegis"), 0));
    CHECK(economy.Spend(FactionId("aegis"), -10));
    CHECK(economy.Stock(FactionId("aegis")) == 0);
}
