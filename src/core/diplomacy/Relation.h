#pragma once

#include <cstdint>

namespace sr::core::diplomacy {

// Three-state diplomatic stance between two factions, or between a tracked actor and a faction
// (see Reputation.h). Ordered Hostile < Neutral < Friendly so callers can threshold a continuous
// score into this enum with a plain comparison.
enum class Relation : std::uint8_t {
    Hostile,
    Neutral,
    Friendly,
};

}  // namespace sr::core::diplomacy
