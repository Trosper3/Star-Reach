#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "core/serialization/KnowledgeSerialization.h"

using sr::core::knowledge::KnowledgeNetwork;
using sr::core::knowledge::KnowledgeStore;
using sr::core::knowledge::NetworkEntryKind;
using sr::core::knowledge::NetworkOwnerKind;
using sr::core::serialization::ByteReader;
using sr::core::serialization::ByteWriter;
using sr::core::serialization::Decode;
using sr::core::serialization::Encode;

TEST_CASE("KnowledgeStore Encode/Decode round-trips every network's contents", "[knowledge]") {
    KnowledgeStore original;
    const auto player = original.Create(NetworkOwnerKind::Player);
    original.Grant(player, NetworkEntryKind::UnlockedBlueprint, "pulse_cannon_i");
    original.Grant(player, NetworkEntryKind::SavedTemplate, "aegis_vanguard");
    original.Grant(player, NetworkEntryKind::DiscoveredSystem, "sol");

    const auto faction = original.Create(NetworkOwnerKind::Faction);
    original.Grant(faction, NetworkEntryKind::SavedTemplate, "reaver_raider");

    ByteWriter writer;
    Encode(writer, original);

    ByteReader reader(writer.Data());
    KnowledgeStore loaded;
    REQUIRE(Decode(reader, loaded));

    const auto* loadedPlayer = loaded.Get(player);
    REQUIRE(loadedPlayer != nullptr);
    CHECK(loadedPlayer->ownerKind == NetworkOwnerKind::Player);
    CHECK(loadedPlayer->unlockedBlueprints.contains("pulse_cannon_i"));
    CHECK(loadedPlayer->savedTemplates.contains("aegis_vanguard"));
    CHECK(loadedPlayer->discoveredSystems.contains("sol"));

    const auto* loadedFaction = loaded.Get(faction);
    REQUIRE(loadedFaction != nullptr);
    CHECK(loadedFaction->ownerKind == NetworkOwnerKind::Faction);
    CHECK(loadedFaction->savedTemplates.contains("reaver_raider"));
}

TEST_CASE("KnowledgeStore Decode preserves the id counter across the round trip", "[knowledge]") {
    KnowledgeStore original;
    original.Create(NetworkOwnerKind::Player);
    original.Create(NetworkOwnerKind::Faction);

    ByteWriter writer;
    Encode(writer, original);
    ByteReader reader(writer.Data());
    KnowledgeStore loaded;
    REQUIRE(Decode(reader, loaded));

    // A fresh id minted after loading must not collide with an id the save already used.
    const auto freshId = loaded.Create(NetworkOwnerKind::Player);
    CHECK(loaded.All().size() == 3);
    CHECK(loaded.Get(freshId) != nullptr);
}

TEST_CASE("KnowledgeStore Decode fails on truncated bytes", "[knowledge]") {
    KnowledgeStore original;
    original.Create(NetworkOwnerKind::Player);

    ByteWriter writer;
    Encode(writer, original);
    std::vector<uint8_t> truncated = writer.Data();
    truncated.resize(truncated.size() / 2);

    ByteReader reader(truncated);
    KnowledgeStore loaded;
    CHECK_FALSE(Decode(reader, loaded));
}

TEST_CASE("Decode rejects an ownerKind byte outside the enum", "[knowledge]") {
    // NetworkOwnerKind has three enumerators; 200 is not one of them. Before GetOwnerKind, this
    // static_cast produced an out-of-range enum that every later switch on it would hit as UB.
    KnowledgeNetwork network;
    network.ownerKind = NetworkOwnerKind::Faction;

    ByteWriter writer;
    Encode(writer, network);
    std::vector<uint8_t> corrupted = writer.Data();
    REQUIRE_FALSE(corrupted.empty());
    corrupted[0] = 200;  // ownerKind is the first byte a network encodes.

    ByteReader reader(corrupted);
    KnowledgeNetwork loaded;
    CHECK_FALSE(Decode(reader, loaded));
}

TEST_CASE("A store whose network carries a corrupt ownerKind is refused whole", "[knowledge]") {
    // The rejection has to reach the store decoder too -- a save that loaded "most of" its
    // networks and silently dropped one is the failure mode a bool return exists to prevent.
    ByteWriter writer;
    writer.Put(std::uint64_t{2});           // next id counter
    writer.Put(std::uint32_t{1});           // network count
    writer.PutString("1");                  // network id
    writer.Put(static_cast<uint8_t>(200));  // ownerKind -- out of range
    writer.Put(std::uint16_t{0});           // unlockedBlueprints
    writer.Put(std::uint16_t{0});           // savedTemplates
    writer.Put(std::uint16_t{0});           // discoveredSystems

    ByteReader reader(writer.Data());
    KnowledgeStore loaded;
    CHECK_FALSE(Decode(reader, loaded));
    CHECK(loaded.All().empty());
}
