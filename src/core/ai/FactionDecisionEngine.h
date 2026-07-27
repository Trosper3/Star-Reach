#pragma once

#include <optional>
#include <string>

#include "core/diplomacy/Territory.h"
#include "core/economy/FactionEconomy.h"
#include "shared/blueprints/Ids.h"

// The Simulation Decision Engine (features.md section 6; architecture.md section 4's
// FactionDecisionSystem row) -- Tier 3 only. features.md section 1.1's LOD table marks Tier 3's
// AI column "4-facet decision engine only," meaning this runs against core/'s galaxy-wide state
// on the macro tick and never against a registry. There is accordingly no modes/space/systems/
// bridge file the way FactionEconomySystem bridges core/economy/ -- nothing here needs a
// per-registry tick, because nothing here reads or writes registry state.
//
// Mode-agnostic, registry-agnostic, and render-agnostic (Law 8) -- sr_core cannot link raylib.
namespace sr::core::ai {

// features.md section 6.1's four operational facets, one profile per faction. Material Security
// is the only facet with a real data source today (core/economy/FactionEconomy's stock) --
// Market Dominance (trade-route profitability), Ideological Doctrine (neighbor stance), and Tech
// Superiority (blueprint/research standing) each need a system this codebase does not have yet
// (a trade-route model, a doctrine-per-faction authoring surface, a tech-tier tracker). They sit
// at a neutral 50 until one exists to compute them for real -- the same "data-only until a
// system reads it" deferral RigFactory.cpp documents for three of the five FacilityKinds.
struct FacetProfile {
    float materialSecurity = 50.0f;     // 0-100. Computed from FactionEconomy::Stock().
    float marketDominance = 50.0f;      // Neutral placeholder -- no trade-route model yet.
    float ideologicalDoctrine = 50.0f;  // Neutral placeholder -- no doctrine data yet.
    float techSuperiority = 50.0f;      // Neutral placeholder -- no tech-tier tracker yet.
};

// A stock level at or above this reads as 100% Material Security; 0 stock reads as 0%. A
// starting point, not a tuned constant -- features.md section 6.3 flags exactly this kind of
// number as needing tools/economy_sim's eventual balancing pass.
inline constexpr int kReferenceStock = 500;

// Computes `faction`'s current facet profile. Material Security is a linear read of its stock
// against kReferenceStock; the other three stay at their neutral default (see FacetProfile).
FacetProfile ComputeFacets(const economy::FactionEconomy& economy, const FactionId& faction);

// features.md section 6.3's one fully-tuned threshold: Material Security below 30 raises a
// resource-raiding fleet's dispatch chance to 45% per macro tick; otherwise 0. Pure -- no RNG
// state -- so the actual roll is the caller's decision (see EvaluateRaidDispatch), which is what
// keeps this testable without faking randomness.
float RaidDispatchChance(const FacetProfile& facets);

// A raid-dispatch decision, without a target system yet -- picking one needs a border/adjacency
// model core/diplomacy/Territory.h does not carry (it records ownership, not neighbors). A
// future system consuming this predates its own producer the same way DockingSystem's
// DockRequest predated player input, per FactionEconomySystem.h's doc comment on that idiom.
struct RaidDispatchDirective {
    FactionId faction;
};

// Rolls RaidDispatchChance(facets) against `roll` (caller-supplied, e.g. from a seeded RNG, so
// results are reproducible in a test and under Law 2's coarse-tick fast-forward). `roll` is
// expected in [0, 1); returns nullopt if it lands at or above the chance, or the chance is 0.
std::optional<RaidDispatchDirective> EvaluateRaidDispatch(const FactionId& faction,
                                                          const FacetProfile& facets, float roll);

// A colonization decision, naming the system to claim.
struct ColonizeDirective {
    FactionId faction;
    std::string systemId;
};

// A faction with at least kReferenceStock in surplus stock (enough to plausibly afford outfitting
// a colony effort) proposes claiming `candidateSystemId`, if it is currently unclaimed. No
// system-discovery or adjacency logic lives here -- Territory.h records ownership, not which
// systems exist or which are reachable -- so `candidateSystemId` is the caller's choice of what
// to check, the same way EvaluateRaidDispatch takes its roll instead of owning an RNG. Returns
// nullopt if the system is already claimed (by anyone, including `faction` itself) or stock is
// insufficient.
std::optional<ColonizeDirective> EvaluateColonization(const FactionId& faction,
                                                      const economy::FactionEconomy& economy,
                                                      const diplomacy::Territory& territory,
                                                      const std::string& candidateSystemId);

}  // namespace sr::core::ai
