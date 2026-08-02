#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/diplomacy/Reputation.h"
#include "core/economy/FactionEconomy.h"
#include "core/events/Intent.h"
#include "core/events/IntentQueue.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/systems/TemplateMarketSystem.h"

using Catch::Approx;
using sr::BlueprintId;
using sr::FactionId;
using sr::KnowledgeNetworkId;
using sr::core::PitchTemplateIntent;
using sr::core::TemplatePayment;
using sr::core::diplomacy::DiplomacyMatrix;
using sr::core::diplomacy::Relation;
using sr::core::diplomacy::Reputation;
using sr::core::economy::FactionEconomy;
using sr::core::knowledge::KnowledgeStore;
using sr::core::knowledge::NetworkOwnerKind;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace template_market_system = sr::space::template_market_system;

namespace {

struct Fixture {
    SystemWorld world{"sol"};
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    FactionEconomy economy;
    DiplomacyMatrix diplomacy;
    Reputation reputation;
    KnowledgeStore knowledge;

    SystemContext Context() {
        SystemContext ctx{world, intents, content, 1.0f / 60.0f, 0};
        ctx.economy = &economy;
        ctx.diplomacy = &diplomacy;
        ctx.reputation = &reputation;
        ctx.knowledge = &knowledge;
        return ctx;
    }

    PitchTemplateIntent MakePitch(KnowledgeNetworkId seller, KnowledgeNetworkId buyer,
                                  FactionId sellerFaction, FactionId buyerFaction) {
        PitchTemplateIntent pitch;
        pitch.sellerNetwork = seller;
        pitch.buyerNetwork = buyer;
        pitch.templateId = BlueprintId("razor_frigate");
        pitch.sellerFaction = sellerFaction;
        pitch.buyerFaction = buyerFaction;
        pitch.materialsCost = 100;
        pitch.archetypeFits = true;
        pitch.beatsCurrentManufacture = true;
        pitch.payment = TemplatePayment::LumpSum;
        pitch.basePayout = 1000.0f;
        return pitch;
    }
};

}  // namespace

TEST_CASE("TemplateMarketSystem refuses a pitch to a Hostile faction", "[template-market]") {
    Fixture fx;
    const KnowledgeNetworkId seller = fx.knowledge.Create(NetworkOwnerKind::Player);
    const KnowledgeNetworkId buyer = fx.knowledge.Create(NetworkOwnerKind::Faction);
    fx.economy.Deposit(FactionId("reavers"), 1000);
    fx.diplomacy.Set(FactionId("reavers"), FactionId("player"), Relation::Hostile);

    fx.intents.Push(fx.MakePitch(seller, buyer, FactionId("player"), FactionId("reavers")));
    template_market_system::Tick(fx.Context());

    CHECK_FALSE(fx.knowledge.Get(buyer)->savedTemplates.count("razor_frigate"));
}

TEST_CASE("TemplateMarketSystem rejects a pitch that does not fit the buyer's archetype",
          "[template-market]") {
    Fixture fx;
    const KnowledgeNetworkId seller = fx.knowledge.Create(NetworkOwnerKind::Player);
    const KnowledgeNetworkId buyer = fx.knowledge.Create(NetworkOwnerKind::Faction);
    fx.economy.Deposit(FactionId("aegis"), 1000);

    PitchTemplateIntent pitch =
        fx.MakePitch(seller, buyer, FactionId("player"), FactionId("aegis"));
    pitch.archetypeFits = false;
    fx.intents.Push(pitch);
    template_market_system::Tick(fx.Context());

    CHECK_FALSE(fx.knowledge.Get(buyer)->savedTemplates.count("razor_frigate"));
    CHECK(fx.economy.Stock(FactionId("aegis")) == 1000);
}

TEST_CASE("TemplateMarketSystem rejects a pitch the buyer cannot afford", "[template-market]") {
    Fixture fx;
    const KnowledgeNetworkId seller = fx.knowledge.Create(NetworkOwnerKind::Player);
    const KnowledgeNetworkId buyer = fx.knowledge.Create(NetworkOwnerKind::Faction);
    fx.economy.Deposit(FactionId("aegis"), 50);  // less than materialsCost (100)

    fx.intents.Push(fx.MakePitch(seller, buyer, FactionId("player"), FactionId("aegis")));
    template_market_system::Tick(fx.Context());

    CHECK_FALSE(fx.knowledge.Get(buyer)->savedTemplates.count("razor_frigate"));
    CHECK(fx.economy.Stock(FactionId("aegis")) == 50);
}

TEST_CASE("An accepted lump-sum pitch copies the Template and pays the seller",
          "[template-market]") {
    Fixture fx;
    const KnowledgeNetworkId seller = fx.knowledge.Create(NetworkOwnerKind::Player);
    const KnowledgeNetworkId buyer = fx.knowledge.Create(NetworkOwnerKind::Faction);
    fx.knowledge.Grant(seller, sr::core::knowledge::NetworkEntryKind::SavedTemplate,
                       "razor_frigate");
    fx.economy.Deposit(FactionId("aegis"), 1000);

    fx.intents.Push(fx.MakePitch(seller, buyer, FactionId("player"), FactionId("aegis")));
    template_market_system::Tick(fx.Context());

    // Neutral relation, default (0) reputation, archetypeFits=true -> rateBonus = 20,
    // payoutMultiplier = 1.1 -- architecture.md 12.7's formula, not a chosen test value.
    CHECK(fx.knowledge.Get(buyer)->savedTemplates.count("razor_frigate") == 1);
    CHECK(fx.economy.Stock(FactionId("aegis")) == 900);    // spent materialsCost
    CHECK(fx.economy.Stock(FactionId("player")) == 1100);  // 1000 base * 1.1 multiplier
}

TEST_CASE("An accepted royalty pitch registers a stream that skims future production",
          "[template-market]") {
    Fixture fx;
    const KnowledgeNetworkId seller = fx.knowledge.Create(NetworkOwnerKind::Player);
    const KnowledgeNetworkId buyer = fx.knowledge.Create(NetworkOwnerKind::Faction);
    fx.economy.Deposit(FactionId("aegis"), 1000);

    PitchTemplateIntent pitch =
        fx.MakePitch(seller, buyer, FactionId("player"), FactionId("aegis"));
    pitch.payment = TemplatePayment::Royalty;
    pitch.basePayout = 0.1f;  // base royalty rate
    fx.intents.Push(pitch);
    template_market_system::Tick(fx.Context());

    // Same 1.1 multiplier as the lump-sum test (identical inputs), applied to the base rate.
    const float expectedRate = 0.1f * 1.1f;
    CHECK(fx.economy.RoyaltyRate(FactionId("aegis"), BlueprintId("razor_frigate")) ==
          Approx(expectedRate));

    // Royalties accrue against Tier 3 production without the player present: a plain Deposit()
    // call (no registry, no player entity) still pays the seller.
    fx.economy.Deposit(FactionId("aegis"), 500);
    const int expectedSkim = static_cast<int>(500.0f * expectedRate);
    CHECK(fx.economy.Stock(FactionId("player")) == expectedSkim);
    CHECK(fx.economy.Stock(FactionId("aegis")) == 900 + (500 - expectedSkim));
}

TEST_CASE("Selling the same Template to two factions at war with each other is permitted",
          "[template-market]") {
    Fixture fx;
    const KnowledgeNetworkId seller = fx.knowledge.Create(NetworkOwnerKind::Player);
    const KnowledgeNetworkId buyerA = fx.knowledge.Create(NetworkOwnerKind::Faction);
    const KnowledgeNetworkId buyerB = fx.knowledge.Create(NetworkOwnerKind::Faction);
    fx.knowledge.Grant(seller, sr::core::knowledge::NetworkEntryKind::SavedTemplate,
                       "razor_frigate");
    fx.economy.Deposit(FactionId("aegis"), 1000);
    fx.economy.Deposit(FactionId("pyre"), 1000);
    fx.diplomacy.Set(FactionId("aegis"), FactionId("pyre"), Relation::Hostile);

    fx.intents.Push(fx.MakePitch(seller, buyerA, FactionId("player"), FactionId("aegis")));
    fx.intents.Push(fx.MakePitch(seller, buyerB, FactionId("player"), FactionId("pyre")));
    template_market_system::Tick(fx.Context());

    CHECK(fx.knowledge.Get(buyerA)->savedTemplates.count("razor_frigate") == 1);
    CHECK(fx.knowledge.Get(buyerB)->savedTemplates.count("razor_frigate") == 1);
}

TEST_CASE("A sold Template survives destruction of the buyer's stations", "[template-market]") {
    Fixture fx;
    const KnowledgeNetworkId seller = fx.knowledge.Create(NetworkOwnerKind::Player);
    const KnowledgeNetworkId buyer = fx.knowledge.Create(NetworkOwnerKind::Faction);
    fx.knowledge.Grant(seller, sr::core::knowledge::NetworkEntryKind::SavedTemplate,
                       "razor_frigate");
    fx.economy.Deposit(FactionId("aegis"), 1000);

    fx.intents.Push(fx.MakePitch(seller, buyer, FactionId("player"), FactionId("aegis")));
    template_market_system::Tick(fx.Context());

    // The sale is a network copy (Law 3) -- there is no entity backing it, so nothing about a
    // buyer's stations dying can undo it.
    entt::registry& registry = fx.world.Registry();
    const entt::entity station = registry.create();
    registry.destroy(station);

    CHECK(fx.knowledge.Get(buyer)->savedTemplates.count("razor_frigate") == 1);
}
