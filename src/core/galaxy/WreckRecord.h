#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "shared/blueprints/Ids.h"
#include "shared/components/Loot.h"
#include "shared/math/Vec2.h"

namespace sr::core::galaxy {

// Durable form of a DeathWreck (Law 3) -- architecture.md section 12.5. LootSystem owns the live
// entity while its system is resident (Tier 1); this is what a demoted system leaves behind so a
// player's recoverable cargo survives sitting in a system they are no longer in, which is the
// common case: they respawn at an allied station, potentially elsewhere, before flying back.
struct WreckRecord {
    std::string systemId;
    Vec2 position;
    std::vector<ModuleId> modules;
    std::vector<ElementStack> elements;

    // Seconds of recovery window left, frozen at whatever it was the instant the system demoted
    // -- nothing simulates a Tier 2/3 system yet (features.md section 1.1), so there is no clock
    // to spend it against while stored here. features.md section 9 leaves wall-clock-vs-in-game
    // time open; freezing on demotion commits to neither.
    float lifetimeSeconds = 0.0f;
};

// Galaxy-wide store of demoted wrecks (Law 8 -- core/galaxy/, not any one registry), the same
// "plain data outside any one registry" shape DiscoveryState uses for per-faction knowledge.
// Keyed by an id assigned at write time rather than by system, since a system can hold more than
// one wreck at once.
class WreckLedger {
public:
    using Id = std::uint64_t;
    static constexpr Id kInvalidId = 0;

    // Stores `record` and returns the id it is later found, removed, or expired by.
    Id Add(WreckRecord record);

    // No-op if `id` is unknown -- already recovered or expired.
    void Remove(Id id);

    const WreckRecord* Find(Id id) const;

    // Ages every stored record by dt and drops any whose recovery window has closed.
    void Tick(float dt);

    // Ids of every currently-stored record for `systemId`. Used by whatever re-instantiates a
    // system's wrecks when it becomes resident again (architecture.md section 12.5) -- there can
    // be more than one, since nothing caps how many times a player dies and leaves before
    // returning.
    std::vector<Id> IdsForSystem(const std::string& systemId) const;

    std::size_t Count() const { return records_.size(); }

private:
    std::unordered_map<Id, WreckRecord> records_;
    Id nextId_ = 1;
};

}  // namespace sr::core::galaxy
