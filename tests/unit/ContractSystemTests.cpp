#include <catch2/catch_test_macros.hpp>

#include "core/diplomacy/Reputation.h"
#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/ContractSystem.h"
#include "shared/components/Contract.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"

using sr::AcceptContractRequest;
using sr::Contract;
using sr::ContractType;
using sr::Destroyed;
using sr::FactionId;
using sr::FactionRef;
using sr::KillCredited;
using sr::Wallet;
using sr::core::diplomacy::Reputation;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace contract_system = sr::space::contract_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt = 1.0f / 60.0f) {
    return SystemContext{world, intents, content, dt, 0};
}

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, Reputation& reputation,
                          float dt = 1.0f / 60.0f) {
    return SystemContext{world, intents, content, dt, 0, nullptr, nullptr, nullptr, &reputation};
}

entt::entity MakeHostileRig(entt::registry& registry, const char* faction) {
    const entt::entity rig = registry.create();
    registry.emplace<FactionRef>(rig, FactionId(faction));
    registry.emplace<Destroyed>(rig);
    return rig;
}

Contract BountyContract(const char* targetFaction, int killsRequired, int rewardCredits,
                        const char* issuingFaction = nullptr) {
    Contract contract;
    contract.type = ContractType::Bounty;
    contract.targetFaction = FactionId(targetFaction);
    contract.killsRequired = killsRequired;
    contract.rewardCredits = rewardCredits;
    if (issuingFaction != nullptr) {
        contract.issuingFaction = FactionId(issuingFaction);
    }
    return contract;
}

Contract EscortContract(entt::entity target, float timeRemaining, int rewardCredits,
                        const char* issuingFaction = nullptr) {
    Contract contract;
    contract.type = ContractType::Escort;
    contract.escortTarget = target;
    contract.timeRemaining = timeRemaining;
    contract.rewardCredits = rewardCredits;
    if (issuingFaction != nullptr) {
        contract.issuingFaction = FactionId(issuingFaction);
    }
    return contract;
}

}  // namespace

TEST_CASE("ContractSystem accepts a request onto a holder with no active contract", "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity holder = registry.create();
    registry.emplace<AcceptContractRequest>(holder, BountyContract("reavers", 1, 100));

    contract_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.all_of<Contract>(holder));
    CHECK(registry.get<Contract>(holder).killsRequired == 1);
    CHECK_FALSE(registry.all_of<AcceptContractRequest>(holder));
}

TEST_CASE("ContractSystem drops a request when the holder already has an active contract",
          "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity holder = registry.create();
    registry.emplace<Contract>(holder, BountyContract("reavers", 5, 100));
    registry.emplace<AcceptContractRequest>(holder, BountyContract("merchants", 1, 999));

    contract_system::Tick(MakeContext(world, intents, content));

    // The original contract survives untouched; the new offer is simply dropped.
    CHECK(registry.get<Contract>(holder).targetFaction == FactionId("reavers"));
    CHECK_FALSE(registry.all_of<AcceptContractRequest>(holder));
}

TEST_CASE("ContractSystem credits a Bounty kill from a matching-faction rig destroyed this tick",
          "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity holder = registry.create();
    registry.emplace<Contract>(holder, BountyContract("reavers", 2, 500));
    MakeHostileRig(registry, "reavers");

    contract_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.all_of<Contract>(holder));  // Not complete yet -- needs 2.
    CHECK(registry.get<Contract>(holder).killsDone == 1);
}

TEST_CASE("ContractSystem ignores a destroyed rig of a non-matching faction", "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity holder = registry.create();
    registry.emplace<Contract>(holder, BountyContract("reavers", 1, 500));
    MakeHostileRig(registry, "merchants");

    contract_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Contract>(holder).killsDone == 0);
}

TEST_CASE("ContractSystem completes a Bounty and pays its reward once killsRequired is met",
          "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity holder = registry.create();
    registry.emplace<Contract>(holder, BountyContract("reavers", 1, 750));
    MakeHostileRig(registry, "reavers");

    contract_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<Contract>(holder));
    REQUIRE(registry.all_of<Wallet>(holder));
    CHECK(registry.get<Wallet>(holder).credits == 750);
}

TEST_CASE("ContractSystem never recounts the same corpse across multiple ticks", "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity holder = registry.create();
    registry.emplace<Contract>(holder, BountyContract("reavers", 100, 1));
    const entt::entity corpse = MakeHostileRig(registry, "reavers");

    contract_system::Tick(MakeContext(world, intents, content));
    contract_system::Tick(MakeContext(world, intents, content));
    contract_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Contract>(holder).killsDone == 1);
    CHECK(registry.all_of<KillCredited>(corpse));
}

TEST_CASE("ContractSystem fails an Escort contract the instant its target is destroyed",
          "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity target = registry.create();
    registry.emplace<Destroyed>(target);

    const entt::entity holder = registry.create();
    registry.emplace<Contract>(holder, EscortContract(target, 60.0f, 500));

    contract_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<Contract>(holder));
    CHECK_FALSE(registry.all_of<Wallet>(holder));
}

TEST_CASE("ContractSystem fails an Escort contract whose target no longer exists at all",
          "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity target = registry.create();
    registry.destroy(target);  // A genuinely stale handle, same technique as elsewhere.

    const entt::entity holder = registry.create();
    registry.emplace<Contract>(holder, EscortContract(target, 60.0f, 500));

    contract_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<Contract>(holder));
}

TEST_CASE(
    "ContractSystem completes an Escort contract once its timer runs out with the target "
    "still alive",
    "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity target = registry.create();  // Alive: no Destroyed tag.

    const entt::entity holder = registry.create();
    registry.emplace<Contract>(holder, EscortContract(target, 1.0f, 500));

    contract_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK_FALSE(registry.all_of<Contract>(holder));
    REQUIRE(registry.all_of<Wallet>(holder));
    CHECK(registry.get<Wallet>(holder).credits == 500);
}

TEST_CASE("ContractSystem keeps a still-running Escort contract active", "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity target = registry.create();

    const entt::entity holder = registry.create();
    registry.emplace<Contract>(holder, EscortContract(target, 60.0f, 500));

    contract_system::Tick(MakeContext(world, intents, content, 1.0f));

    REQUIRE(registry.all_of<Contract>(holder));
    CHECK(registry.get<Contract>(holder).timeRemaining < 60.0f);
}

TEST_CASE("ContractSystem raises the issuer's reputation when a Bounty contract completes",
          "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    Reputation reputation;

    const entt::entity holder = registry.create();
    registry.emplace<Contract>(holder, BountyContract("reavers", 1, 750, "aegis"));
    MakeHostileRig(registry, "reavers");

    contract_system::Tick(MakeContext(world, intents, content, reputation));

    CHECK(reputation.Score(FactionId("aegis")) > 0.0f);
}

TEST_CASE("ContractSystem lowers the issuer's reputation when an Escort contract fails",
          "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    Reputation reputation;

    const entt::entity target = registry.create();
    registry.emplace<Destroyed>(target);

    const entt::entity holder = registry.create();
    registry.emplace<Contract>(holder, EscortContract(target, 60.0f, 500, "aegis"));

    contract_system::Tick(MakeContext(world, intents, content, reputation));

    CHECK(reputation.Score(FactionId("aegis")) < 0.0f);
}

TEST_CASE("ContractSystem raises the issuer's reputation when an Escort contract completes",
          "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    Reputation reputation;

    const entt::entity target = registry.create();

    const entt::entity holder = registry.create();
    registry.emplace<Contract>(holder, EscortContract(target, 1.0f, 500, "aegis"));

    contract_system::Tick(MakeContext(world, intents, content, reputation, 1.0f));

    CHECK(reputation.Score(FactionId("aegis")) > 0.0f);
}

TEST_CASE("ContractSystem leaves reputation untouched when issuingFaction was never set",
          "[contract]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    Reputation reputation;

    const entt::entity holder = registry.create();
    registry.emplace<Contract>(holder, BountyContract("reavers", 1, 750));  // No issuing faction.
    MakeHostileRig(registry, "reavers");

    contract_system::Tick(MakeContext(world, intents, content, reputation));

    CHECK(reputation.Score(FactionId("")) == 0.0f);
}
