#pragma once

#include <optional>
#include <string>
#include <vector>

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

// features.md 5.1's Three Pillars -- a faction remains active as long as it holds at least one.
// One implementation applied identically to every faction, the player's included: features.md
// 3.3's Hard Game Over is this same predicate on the player's own faction (architecture.md
// 12.3's "the player is not a special case").
struct SurvivalPillars {
    bool commandStructure = false;
    bool leadership = false;
    bool economicFootprint = false;
};

// `hasCommandStructure` (at least one station/capital carrying a command module) and
// `hasLeadership` (the faction head or an AI sub-commander, architecture.md #82, still alive)
// are supplied by the caller rather than queried here: answering either needs a registry, and
// Tier 3 -- this runs on the same macro tick as ComputeFacets/EvaluateColonization -- has none
// (features.md 1.1). The same "caller supplies what core/ can't compute" shape
// EvaluateRaidDispatch's `roll` and EvaluateColonization's `candidateSystemId` already use.
// Economic Footprint is the one pillar with a real Tier-3-native data source (settled,
// architecture.md 12.3): any lifetime production at all, economy.TotalProduction(faction) > 0 --
// not a threshold, not territory alone.
SurvivalPillars EvaluateSurvival(const economy::FactionEconomy& economy, const FactionId& faction,
                                 bool hasCommandStructure, bool hasLeadership);

// True only when every pillar is lost -- collapse (features.md 5.1), irreversible. Separate from
// SurvivalPillars itself so a caller/test can assert on one pillar without also computing
// collapse.
bool HasCollapsed(const SurvivalPillars& pillars);

// Applies collapse's one Tier-3-native consequence: every system `faction` holds becomes
// unclaimed (features.md 5.1). The other named consequence -- surviving ships scattering into
// rogue scavenger groups -- needs registry access this file does not have (Tier 3) and is a
// future Tier 1 consumer's job to wire up, the same "correctly-shaped, independently-tested
// bridge, no real caller yet" shape architecture.md 12.5's WreckRecord landed in before its
// eventual warp-system wiring (#97) existed.
void CollapseFaction(diplomacy::Territory& territory, const FactionId& faction);

// features.md 5.7: the Reapers target by structural density, not politics -- a distinct AI-
// targeting axis nothing else in the design uses. A candidate system and the caller-computed
// density score (station count and grade, once core/galaxy/ has a per-system roster to compute it
// from) -- not owned here for the same reason EvaluateColonization takes candidateSystemId
// instead of discovering it (architecture.md 12.32).
struct ReaperTargetCandidate {
    std::string systemId;
    float structuralDensity;  // Caller's units; only relative order matters here.
};

// Highest-density candidate wins, full stop -- no relation check, no roll. Isolated or derelict
// sectors (features.md 5.7) score at or near zero and simply never win. A tie keeps whichever
// candidate appears first in `candidates`. nullopt only for an empty list.
std::optional<std::string> SelectReaperTarget(const std::vector<ReaperTargetCandidate>& candidates);

}  // namespace sr::core::ai
