#include <catch2/catch_test_macros.hpp>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/economy/FactionEconomy.h"
#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/FactionEconomySystem.h"
#include "shared/components/Economy.h"

using sr::DepositRequest;
using sr::FactionId;
using sr::SpendRequest;
using sr::SpendResult;
using sr::TradePressureRequest;
using sr::core::diplomacy::DiplomacyMatrix;
using sr::core::diplomacy::Relation;
using sr::core::economy::FactionEconomy;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace faction_economy_system = sr::space::faction_economy_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, FactionEconomy& economy) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0, &economy};
}

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, DiplomacyMatrix& diplomacy) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0, nullptr, nullptr, &diplomacy};
}

}  // namespace

TEST_CASE("FactionEconomySystem credits a DepositRequest and clears it", "[faction-economy]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    FactionEconomy economy;

    const entt::entity self = registry.create();
    registry.emplace<DepositRequest>(self, FactionId("aegis"), 200);

    faction_economy_system::Tick(MakeContext(world, intents, content, economy));

    CHECK(economy.Stock(FactionId("aegis")) == 200);
    CHECK_FALSE(registry.all_of<DepositRequest>(self));
}

TEST_CASE("FactionEconomySystem grants a successful SpendRequest and writes SpendResult",
          "[faction-economy]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    FactionEconomy economy;
    economy.Deposit(FactionId("aegis"), 500);

    const entt::entity self = registry.create();
    registry.emplace<SpendRequest>(self, FactionId("aegis"), 300);

    faction_economy_system::Tick(MakeContext(world, intents, content, economy));

    REQUIRE(registry.all_of<SpendResult>(self));
    CHECK(registry.get<SpendResult>(self).succeeded);
    CHECK(economy.Stock(FactionId("aegis")) == 200);
    CHECK_FALSE(registry.all_of<SpendRequest>(self));
}

TEST_CASE("FactionEconomySystem refuses a SpendRequest the faction cannot afford",
          "[faction-economy]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    FactionEconomy economy;
    economy.Deposit(FactionId("aegis"), 100);

    const entt::entity self = registry.create();
    registry.emplace<SpendRequest>(self, FactionId("aegis"), 300);

    faction_economy_system::Tick(MakeContext(world, intents, content, economy));

    REQUIRE(registry.all_of<SpendResult>(self));
    CHECK_FALSE(registry.get<SpendResult>(self).succeeded);
    CHECK(economy.Stock(FactionId("aegis")) == 100);  // Untouched.
}

TEST_CASE("FactionEconomySystem does nothing when the context has no economy pointer",
          "[faction-economy]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity self = registry.create();
    registry.emplace<DepositRequest>(self, FactionId("aegis"), 200);

    const SystemContext ctx{world, intents, content, 1.0f / 60.0f, 0};  // economy defaults null.
    faction_economy_system::Tick(ctx);

    CHECK(registry.all_of<DepositRequest>(self));  // Left alone, not silently dropped.
}

TEST_CASE("FactionEconomySystem drifts a pair one band toward Hostile per blockade request",
          "[faction-economy]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    const entt::entity self = registry.create();
    registry.emplace<TradePressureRequest>(self, FactionId("aegis"), FactionId("reavers"), true);

    faction_economy_system::Tick(MakeContext(world, intents, content, diplomacy));

    CHECK(diplomacy.Get(FactionId("aegis"), FactionId("reavers")) == Relation::Distrustful);
    CHECK_FALSE(registry.all_of<TradePressureRequest>(self));
}

TEST_CASE(
    "FactionEconomySystem's sustained blockade pressure drifts a pair to Hostile and no "
    "further",
    "[faction-economy]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    const entt::entity self = registry.create();
    for (int i = 0; i < 10; ++i) {
        registry.emplace_or_replace<TradePressureRequest>(self, FactionId("aegis"),
                                                          FactionId("reavers"), true);
        faction_economy_system::Tick(MakeContext(world, intents, content, diplomacy));
    }

    CHECK(diplomacy.Get(FactionId("aegis"), FactionId("reavers")) == Relation::Hostile);
}

TEST_CASE("FactionEconomySystem drifts a pair one band toward Allied per trade request",
          "[faction-economy]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    const entt::entity self = registry.create();
    registry.emplace<TradePressureRequest>(self, FactionId("aegis"), FactionId("meridian"), false);

    faction_economy_system::Tick(MakeContext(world, intents, content, diplomacy));

    CHECK(diplomacy.Get(FactionId("aegis"), FactionId("meridian")) == Relation::Friendly);
}

TEST_CASE(
    "FactionEconomySystem does nothing with trade pressure when the context has no "
    "diplomacy pointer",
    "[faction-economy]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity self = registry.create();
    registry.emplace<TradePressureRequest>(self, FactionId("aegis"), FactionId("reavers"), true);

    const SystemContext ctx{world, intents, content, 1.0f / 60.0f, 0};  // diplomacy defaults null.
    faction_economy_system::Tick(ctx);

    CHECK(registry.all_of<TradePressureRequest>(self));  // Left alone, not silently dropped.
}
