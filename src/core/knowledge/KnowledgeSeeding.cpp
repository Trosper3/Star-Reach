#include "core/knowledge/KnowledgeSeeding.h"

#include <array>
#include <utility>

namespace sr::core::knowledge {
namespace {

// The same ten factions core/diplomacy/RelationSeeding.cpp's kAlliances/kRivalries enumerate --
// every faction that can hold discovered-system knowledge, not just the ones in a baseline table.
constexpr std::array<const char*, 10> kAllFactions{
    "aegis_directorate", "meridian_star_corps", "kore_industries",
    "the_forgotten",     "ai_concordance",      "zenith_collective",
    "voidwalkers",       "edenian_pact",        "pyre_ascendancy",
    "reapers",
};

}  // namespace

void SeedFactionNetworks(KnowledgeStore& store) {
    for (const char* faction : kAllFactions) {
        KnowledgeNetwork network;
        network.ownerKind = NetworkOwnerKind::Faction;
        store.LoadNetwork(FactionNetworkId(FactionId(faction)), std::move(network));
    }
}

}  // namespace sr::core::knowledge
