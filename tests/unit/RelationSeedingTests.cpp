#include <catch2/catch_test_macros.hpp>

#include <array>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/diplomacy/RelationSeeding.h"

using sr::FactionId;
using sr::core::diplomacy::DiplomacyMatrix;
using sr::core::diplomacy::Relation;
using sr::core::diplomacy::SeedBaselineRelations;
using sr::core::diplomacy::SeedReaperHostility;

namespace {

constexpr std::array<const char*, 10> kAllFactions{
    "aegis_directorate", "meridian_star_corps", "kore_industries",
    "the_forgotten",     "ai_concordance",      "pyre_ascendancy",
    "voidwalkers",       "zenith_collective",   "edenian_pact",
    "reapers",
};

}  // namespace

TEST_CASE("SeedBaselineRelations is symmetric for every seeded pair", "[diplomacy][seeding]") {
    DiplomacyMatrix matrix;
    SeedBaselineRelations(matrix);

    for (const char* a : kAllFactions) {
        for (const char* b : kAllFactions) {
            CHECK(matrix.Get(FactionId(a), FactionId(b)) == matrix.Get(FactionId(b), FactionId(a)));
        }
    }
}

TEST_CASE("SeedBaselineRelations gives every faction exactly one Allied and three Hostile",
          "[diplomacy][seeding]") {
    DiplomacyMatrix matrix;
    SeedBaselineRelations(matrix);

    for (const char* faction : kAllFactions) {
        int allies = 0;
        int rivals = 0;
        for (const char* other : kAllFactions) {
            if (other == faction) {
                continue;
            }
            const Relation relation = matrix.Get(FactionId(faction), FactionId(other));
            if (relation == Relation::Allied) {
                ++allies;
            } else if (relation == Relation::Hostile) {
                ++rivals;
            }
        }
        CHECK(allies == 1);
        CHECK(rivals == 3);
    }
}

TEST_CASE("SeedBaselineRelations never marks a pair both allied and rival",
          "[diplomacy][seeding]") {
    // Disjointness follows directly from Set() overwriting a single cell per pair, but the
    // baseline table itself (features.md 5.4-5.6) is hand-authored and this is the check that
    // would catch an accidental duplicate entry.
    DiplomacyMatrix matrix;
    SeedBaselineRelations(matrix);

    int alliedPairs = 0;
    int hostilePairs = 0;
    for (std::size_t i = 0; i < kAllFactions.size(); ++i) {
        for (std::size_t j = i + 1; j < kAllFactions.size(); ++j) {
            const Relation relation =
                matrix.Get(FactionId(kAllFactions[i]), FactionId(kAllFactions[j]));
            if (relation == Relation::Allied) {
                ++alliedPairs;
            } else if (relation == Relation::Hostile) {
                ++hostilePairs;
            }
        }
    }
    CHECK(alliedPairs == 5);
    CHECK(hostilePairs == 15);
}

TEST_CASE("SeedReaperHostility escalates the Reapers' three rivals to War",
          "[diplomacy][seeding]") {
    DiplomacyMatrix matrix;
    SeedBaselineRelations(matrix);
    SeedReaperHostility(matrix);

    const FactionId reapers("reapers");
    CHECK(matrix.Get(FactionId("aegis_directorate"), reapers) == Relation::War);
    CHECK(matrix.Get(FactionId("the_forgotten"), reapers) == Relation::War);
    CHECK(matrix.Get(FactionId("ai_concordance"), reapers) == Relation::War);
}

TEST_CASE("SeedReaperHostility leaves the Pyre alliance untouched", "[diplomacy][seeding]") {
    DiplomacyMatrix matrix;
    SeedBaselineRelations(matrix);
    SeedReaperHostility(matrix);

    CHECK(matrix.Get(FactionId("pyre_ascendancy"), FactionId("reapers")) == Relation::Allied);
}

TEST_CASE("SeedReaperHostility makes every unlisted faction Hostile with the Reapers",
          "[diplomacy][seeding]") {
    DiplomacyMatrix matrix;
    SeedBaselineRelations(matrix);
    SeedReaperHostility(matrix);

    const FactionId reapers("reapers");
    for (const char* faction : {"meridian_star_corps", "kore_industries", "voidwalkers",
                                "zenith_collective", "edenian_pact"}) {
        CHECK(matrix.Get(FactionId(faction), reapers) == Relation::Hostile);
    }
}

TEST_CASE("SeedReaperHostility's escalation is exclusive to the Reapers, not every rivalry",
          "[diplomacy][seeding]") {
    // The other nine factions' own fifteen rivalries (Reapers' excluded) stay plain Hostile --
    // the War escalation is not a property of "being someone's rival" in general.
    DiplomacyMatrix matrix;
    SeedBaselineRelations(matrix);
    SeedReaperHostility(matrix);

    CHECK(matrix.Get(FactionId("aegis_directorate"), FactionId("the_forgotten")) ==
          Relation::Hostile);
    CHECK(matrix.Get(FactionId("aegis_directorate"), FactionId("pyre_ascendancy")) ==
          Relation::Hostile);
    CHECK(matrix.Get(FactionId("meridian_star_corps"), FactionId("kore_industries")) ==
          Relation::Hostile);
}
