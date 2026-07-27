#pragma once

#include <unordered_map>

#include "shared/blueprints/Ids.h"

namespace sr::core::economy {

// A faction's spendable stock -- galaxy-wide state (Law 2, hard rule 2), so it lives here in
// core/ rather than in any one entt::registry: a colonization/rebuild decision needs to see a
// faction's TOTAL stock across every system it controls, not just whichever one is ticking.
//
// Mode-agnostic, registry-agnostic, and render-agnostic (Law 8) -- sr_core cannot link raylib,
// which is the mechanism keeping this honest, and what makes this class constructible and
// testable with no window, no registry, and no mode.
//
// One scalar per faction rather than legacy StarReach2's per-station, per-good StationEconomy
// (materials/items each with their own stock and a scarcity price curve) -- there is no
// tradeable-goods content pipeline or station blueprint yet (StationFactory, #41) for per-good
// granularity to attach to.
class FactionEconomy {
public:
    int Stock(const FactionId& faction) const;

    // Production/income. amount <= 0 is a no-op.
    void Deposit(const FactionId& faction, int amount);

    // Transactional, all-or-nothing -- ported from legacy StarReach2's SpendFactionStock
    // contract. Returns false and changes nothing if the faction cannot afford `amount`.
    // amount <= 0 always succeeds without touching the stock.
    bool Spend(const FactionId& faction, int amount);

private:
    std::unordered_map<FactionId, int> stock_;
};

}  // namespace sr::core::economy
