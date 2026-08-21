#include "core/diplomacy/RelationSeeding.h"

#include <array>
#include <utility>

namespace sr::core::diplomacy {
namespace {

using Pair = std::pair<FactionId, FactionId>;

// features.md 5.5's five alliances -- a perfect matching, all ten factions paired.
constexpr std::array<std::pair<const char*, const char*>, 5> kAlliances{{
    {"aegis_directorate", "meridian_star_corps"},
    {"kore_industries", "the_forgotten"},
    {"ai_concordance", "zenith_collective"},
    {"voidwalkers", "edenian_pact"},
    {"pyre_ascendancy", "reapers"},
}};

// features.md 5.6's fifteen rivalries.
constexpr std::array<std::pair<const char*, const char*>, 15> kRivalries{{
    {"aegis_directorate", "the_forgotten"},
    {"aegis_directorate", "pyre_ascendancy"},
    {"aegis_directorate", "reapers"},
    {"meridian_star_corps", "kore_industries"},
    {"meridian_star_corps", "voidwalkers"},
    {"meridian_star_corps", "edenian_pact"},
    {"kore_industries", "edenian_pact"},
    {"kore_industries", "ai_concordance"},
    {"the_forgotten", "pyre_ascendancy"},
    {"the_forgotten", "reapers"},
    {"ai_concordance", "voidwalkers"},
    {"ai_concordance", "reapers"},
    {"zenith_collective", "pyre_ascendancy"},
    {"zenith_collective", "voidwalkers"},
    {"zenith_collective", "edenian_pact"},
}};

// features.md 5.7: every faction not already an ally or rival of the Reapers still sits at
// Hostile with them.
constexpr std::array<const char*, 5> kReaperUnlistedHostiles{
    "meridian_star_corps", "kore_industries", "voidwalkers", "zenith_collective", "edenian_pact",
};

}  // namespace

void SeedBaselineRelations(DiplomacyMatrix& matrix) {
    for (const auto& [a, b] : kAlliances) {
        matrix.Set(FactionId(a), FactionId(b), Relation::Allied);
    }
    for (const auto& [a, b] : kRivalries) {
        matrix.Set(FactionId(a), FactionId(b), Relation::Hostile);
    }
}

void SeedReaperHostility(DiplomacyMatrix& matrix) {
    const FactionId reapers("reapers");

    // The Reapers' three table rivals are priority targets, not the limit of their hostility --
    // escalate the plain Hostile SeedBaselineRelations already wrote for these three specifically.
    matrix.Set(FactionId("aegis_directorate"), reapers, Relation::War);
    matrix.Set(FactionId("the_forgotten"), reapers, Relation::War);
    matrix.Set(FactionId("ai_concordance"), reapers, Relation::War);

    for (const char* faction : kReaperUnlistedHostiles) {
        matrix.Set(FactionId(faction), reapers, Relation::Hostile);
    }
}

}  // namespace sr::core::diplomacy
