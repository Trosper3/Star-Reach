#include "core/diplomacy/DiplomacyMatrix.h"

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

}  // namespace sr::core::diplomacy
