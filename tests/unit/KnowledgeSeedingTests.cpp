#include <catch2/catch_test_macros.hpp>

#include <array>

#include "core/knowledge/KnowledgeSeeding.h"

using sr::FactionId;
using sr::core::knowledge::FactionNetworkId;
using sr::core::knowledge::KnowledgeStore;
using sr::core::knowledge::NetworkOwnerKind;
using sr::core::knowledge::SeedFactionNetworks;

namespace {

constexpr std::array<const char*, 10> kAllFactions{
    "aegis_directorate", "meridian_star_corps", "kore_industries",
    "the_forgotten",     "ai_concordance",      "pyre_ascendancy",
    "voidwalkers",       "zenith_collective",   "edenian_pact",
    "reapers",
};

}  // namespace

TEST_CASE("SeedFactionNetworks registers all ten factions", "[knowledge][seeding]") {
    KnowledgeStore store;
    SeedFactionNetworks(store);

    for (const char* faction : kAllFactions) {
        const auto* network = store.Get(FactionNetworkId(FactionId(faction)));
        REQUIRE(network != nullptr);
        CHECK(network->ownerKind == NetworkOwnerKind::Faction);
        CHECK(network->discoveredSystems.empty());
    }
}

TEST_CASE("SeedFactionNetworks does not register an unlisted faction", "[knowledge][seeding]") {
    KnowledgeStore store;
    SeedFactionNetworks(store);

    CHECK(store.Get(FactionNetworkId(FactionId("reavers"))) == nullptr);
}

TEST_CASE("A faction's network accepts a discovered-system grant after seeding",
          "[knowledge][seeding]") {
    KnowledgeStore store;
    SeedFactionNetworks(store);

    const auto id = FactionNetworkId(FactionId("aegis_directorate"));
    store.Grant(id, sr::core::knowledge::NetworkEntryKind::DiscoveredSystem, "sol");

    CHECK(store.Get(id)->discoveredSystems.contains("sol"));
}
