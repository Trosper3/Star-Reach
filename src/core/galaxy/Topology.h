#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/galaxy/Seeding.h"
#include "shared/blueprints/Ids.h"

// core/galaxy/Topology -- the missing prerequisite architecture.md 12.17 names: systems are
// identified by a string id ("sol") and carry no galactic coordinate at all, so
// core/galaxy/Seeding.h's cascade has nothing to derive from. This is the coordinate <-> id <->
// record store that gives every system a grid coordinate.
//
// Mode-agnostic, registry-agnostic, render-agnostic -- sr_core links no raylib (Law 8) -- and a
// store, not a ticking system (like core/knowledge/KnowledgeNetwork.h): nothing about it runs per
// frame.
namespace sr::core::galaxy {

// A system's grid coordinate in its galaxy -- the same shape Seeding.h's cascade already
// consumes at the "solar system within its galaxy" level (architecture.md 12.15's coordinate
// hierarchy), so a Topology coordinate feeds straight into DeriveSolarSystemSeed with no
// conversion.
using SystemCoord = Coordinate;

// A system's identity, derived from its coordinate rather than authored (architecture.md 12.17:
// "a system's identity IS its location" -- systems are procedurally generated from position, so
// position is the only stable identity available; a display name is layered on top, assigned on
// discovery and freely changed).
using SystemId = std::uint64_t;

// IdFromCoord/CoordFromId are a lossless bit-pack, not a hash: SystemId must round-trip back to
// the coordinate it came from, which a mixing function (SplitMix64, used for seeds, never
// identity) cannot guarantee. `x` occupies the high 32 bits, `y` the low 32 bits, each
// reinterpreted as its unsigned bit pattern -- the same packing convention Seeding.h's own
// Coordinate comment documents, so the two stay consistent without sharing code. C++20 guarantees
// two's-complement signed integers and a defined signed<->unsigned conversion, so this produces
// the identical id on every platform.
SystemId IdFromCoord(SystemCoord coord);
SystemCoord CoordFromId(SystemId id);

// The per-system state features.md 7.2 lists as persisted -- ownership, built/destroyed/captured
// stations, depletion, and everything else a player or faction could have changed, none of which
// is seed-derived. A record exists only for a system something has touched (architecture.md
// 12.17); a never-visited system is seed output and costs nothing to represent.
struct SystemRecord {
    SystemCoord coord;
    std::string displayName;  // Assigned on discovery; freely renameable, never identity.
    FactionId claimedBy;      // Default-constructed (empty()) means unclaimed.
};

// The galaxy's coordinate <-> id <-> record store.
//
// ⚠️ The macro tick (features.md 9.1) must iterate Records(), never the coordinate space -- with
// that held, a billion-system galaxy costs what a two-thousand-system one costs. One wrong loop
// over coordinates instead of records caps galaxy size permanently and presents only as "the game
// got slow" (architecture.md 12.17).
class Topology {
public:
    // Records exist only for systems something has touched -- nullptr for any other id, which is
    // the normal case for the overwhelming majority of a large galaxy's coordinate space.
    const SystemRecord* Find(SystemId id) const;

    // Returns the existing record for `coord`, or default-constructs and stores one (empty
    // display name, unclaimed) and returns that. This is the one write path -- "touching" a
    // system is calling this.
    SystemRecord& GetOrCreate(SystemCoord coord);

    // No-op if `id` has no record -- a system reverting to pure seed output (nothing has ever
    // touched it, or its record demoted away) is a normal state, not a bug to guard against.
    void Erase(SystemId id);

    // The 8-connected grid cells around `coord`. Symmetric by construction: if B is a neighbour
    // of A, A is a neighbour of B. This is also today's warp-route adjacency -- nothing yet
    // authors routes that diverge from the grid (architecture.md 12.17's Types table; the fuel-
    // gated route model is a later, separate task), so the two coincide until one does.
    std::vector<SystemCoord> Neighbors(SystemCoord coord) const;

    // The macro tick's iteration surface (see the class-level warning above) and what a future
    // serializer walks -- ordinary callers use Find()/GetOrCreate()/Erase() instead.
    const std::unordered_map<SystemId, SystemRecord>& Records() const { return records_; }

private:
    std::unordered_map<SystemId, SystemRecord> records_;
};

}  // namespace sr::core::galaxy
