#include "core/diplomacy/DiplomacyMatrix.h"

#include <cstdint>

namespace sr::core::diplomacy {

DiplomacyMatrix::Pair DiplomacyMatrix::Canonical(const FactionId& a, const FactionId& b) {
    return a <= b ? Pair(a, b) : Pair(b, a);
}

Relation DiplomacyMatrix::Get(const FactionId& a, const FactionId& b) const {
    if (a == b) {
        return Relation::Friendly;
    }
    const auto it = relations_.find(Canonical(a, b));
    return it != relations_.end() ? it->second : Relation::Neutral;
}

void DiplomacyMatrix::Set(const FactionId& a, const FactionId& b, Relation relation) {
    if (a == b) {
        return;
    }
    relations_[Canonical(a, b)] = relation;
}

void DiplomacyMatrix::DriftToward(const FactionId& a, const FactionId& b, Relation target) {
    const auto current = static_cast<std::uint8_t>(Get(a, b));
    const auto want = static_cast<std::uint8_t>(target);
    if (current == want) {
        return;
    }
    const std::uint8_t stepped = current < want ? current + 1 : current - 1;
    Set(a, b, static_cast<Relation>(stepped));
}

}  // namespace sr::core::diplomacy
