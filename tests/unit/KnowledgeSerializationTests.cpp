#include <catch2/catch_test_macros.hpp>

#include "core/serialization/KnowledgeSerialization.h"

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
