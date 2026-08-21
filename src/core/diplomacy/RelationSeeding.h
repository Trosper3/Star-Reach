#pragma once

#include "core/diplomacy/DiplomacyMatrix.h"

// The DiplomacyMatrix seeders, architecture.md 12.32 -- DiplomacyMatrix::Get established the
// matrix has zero writers anywhere; this is where the baseline actually gets written.
namespace sr::core::diplomacy {

// Writes features.md 5.4's twenty pairs: five Relation::Allied (the alliances, 5.5), fifteen
// Relation::Hostile (the rivalries, 5.6 -- the standard severity; the Reapers' own three are
// escalated by SeedReaperHostility below). Idempotent -- safe to call once at startup.
void SeedBaselineRelations(DiplomacyMatrix& matrix);

// features.md 5.7's exception, called immediately after SeedBaselineRelations. Two effects:
// (1) escalates the Reapers' three seeded rivals -- Aegis Directorate, The Forgotten, AI
// Concordance -- from Relation::Hostile to Relation::War, the priority-target tier that "absorbs
// the most pressure"; (2) every faction not already covered by the Reapers' one ally and three
// rivals (five factions -- Meridian Star Corps, Kore Industries, Voidwalkers, Zenith Collective,
// Edenian Pact) gets an explicit Relation::Hostile against FactionId("reapers"), overriding Get()'s
// Neutral default the same way every other baseline entry does.
void SeedReaperHostility(DiplomacyMatrix& matrix);

}  // namespace sr::core::diplomacy
