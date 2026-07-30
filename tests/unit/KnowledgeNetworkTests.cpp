#include <catch2/catch_test_macros.hpp>

#include "core/knowledge/KnowledgeNetwork.h"

using sr::core::knowledge::KnowledgeStore;
using sr::core::knowledge::NetworkEntryKind;
using sr::core::knowledge::NetworkOwnerKind;

TEST_CASE("KnowledgeStore::Create returns a network with the requested owner kind", "[knowledge]") {
    KnowledgeStore store;
    const auto id = store.Create(NetworkOwnerKind::Player);

    const auto* network = store.Get(id);
    REQUIRE(network != nullptr);
    CHECK(network->ownerKind == NetworkOwnerKind::Player);
    CHECK(network->unlockedBlueprints.empty());
    CHECK(network->savedTemplates.empty());
    CHECK(network->discoveredSystems.empty());
}

TEST_CASE("KnowledgeStore::Create hands out distinct ids", "[knowledge]") {
    KnowledgeStore store;
    const auto a = store.Create(NetworkOwnerKind::Player);
    const auto b = store.Create(NetworkOwnerKind::Faction);
    CHECK_FALSE(a == b);
}

TEST_CASE("KnowledgeStore::Get returns nullptr for an unknown id", "[knowledge]") {
    KnowledgeStore store;
    CHECK(store.Get(sr::KnowledgeNetworkId("nope")) == nullptr);
}

TEST_CASE("KnowledgeStore::Grant round-trips into the matching category", "[knowledge]") {
    KnowledgeStore store;
    const auto id = store.Create(NetworkOwnerKind::Player);

    store.Grant(id, NetworkEntryKind::UnlockedBlueprint, "pulse_cannon_i");
    store.Grant(id, NetworkEntryKind::SavedTemplate, "aegis_vanguard");
    store.Grant(id, NetworkEntryKind::DiscoveredSystem, "sol");

    const auto* network = store.Get(id);
    REQUIRE(network != nullptr);
    CHECK(network->unlockedBlueprints.contains("pulse_cannon_i"));
    CHECK(network->savedTemplates.contains("aegis_vanguard"));
    CHECK(network->discoveredSystems.contains("sol"));
}

TEST_CASE("KnowledgeStore::Grant is a no-op for an unknown network id", "[knowledge]") {
    KnowledgeStore store;
    // Must not crash or create an entry as a side effect.
    store.Grant(sr::KnowledgeNetworkId("ghost"), NetworkEntryKind::UnlockedBlueprint, "x");
    CHECK(store.Get(sr::KnowledgeNetworkId("ghost")) == nullptr);
}

TEST_CASE("KnowledgeStore::Copy moves a saved Template from one network to another",
          "[knowledge]") {
    KnowledgeStore store;
    const auto seller = store.Create(NetworkOwnerKind::Faction);
    const auto buyer = store.Create(NetworkOwnerKind::Faction);
    store.Grant(seller, NetworkEntryKind::SavedTemplate, "aegis_vanguard");

    store.Copy(seller, buyer, "aegis_vanguard");

    CHECK(store.Get(seller)->savedTemplates.contains("aegis_vanguard"));
    CHECK(store.Get(buyer)->savedTemplates.contains("aegis_vanguard"));
}

TEST_CASE("KnowledgeStore::Copy is a no-op when the source lacks the Template", "[knowledge]") {
    KnowledgeStore store;
    const auto seller = store.Create(NetworkOwnerKind::Faction);
    const auto buyer = store.Create(NetworkOwnerKind::Faction);

    store.Copy(seller, buyer, "not_owned");

    CHECK(store.Get(buyer)->savedTemplates.empty());
}

TEST_CASE("KnowledgeStore::Copy is a no-op when either id is unknown", "[knowledge]") {
    KnowledgeStore store;
    const auto real = store.Create(NetworkOwnerKind::Faction);
    store.Grant(real, NetworkEntryKind::SavedTemplate, "aegis_vanguard");
    const sr::KnowledgeNetworkId ghost("ghost");

    store.Copy(real, ghost, "aegis_vanguard");  // Destination unknown.
    store.Copy(ghost, real, "aegis_vanguard");  // Source unknown -- must not duplicate.

    CHECK(store.Get(real)->savedTemplates.size() == 1);
}

TEST_CASE("KnowledgeStore::Destroy removes only the targeted network", "[knowledge]") {
    KnowledgeStore store;
    const auto a = store.Create(NetworkOwnerKind::Player);
    const auto b = store.Create(NetworkOwnerKind::Commander);
    store.Grant(b, NetworkEntryKind::UnlockedBlueprint, "keep_me");

    store.Destroy(a);

    CHECK(store.Get(a) == nullptr);
    REQUIRE(store.Get(b) != nullptr);
    CHECK(store.Get(b)->unlockedBlueprints.contains("keep_me"));
}

TEST_CASE("KnowledgeStore::Destroy is a no-op for an unknown id", "[knowledge]") {
    KnowledgeStore store;
    store.Destroy(sr::KnowledgeNetworkId("nope"));  // Must not crash.
    SUCCEED();
}
