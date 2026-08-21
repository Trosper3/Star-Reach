#pragma once

#include <cstdint>

namespace sr::core::diplomacy {

// Six-state diplomatic stance between two factions, or between a tracked actor and a faction
// (see Reputation.h), features.md 5.3. Ordered War < Hostile < Distrustful < Neutral < Friendly <
// Allied so callers can threshold a continuous score into this enum with a plain comparison --
// the same property the old three-state version documented, extended rather than dropped.
enum class Relation : std::uint8_t {
    War,
    Hostile,
    Distrustful,
    Neutral,
    Friendly,
    Allied,
};

}  // namespace sr::core::diplomacy
