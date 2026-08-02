#pragma once

#include <unordered_map>
#include <vector>

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

    // Production/income. amount <= 0 is a no-op. Skims every royalty stream `faction` owes
    // (below) before crediting the remainder to `faction` itself -- the mechanism behind
    // architecture.md 12.7's "royalties accrue against Tier 3 production without the player
    // present": whatever calls Deposit() to credit a faction's abstract production already runs
    // with no player or registry involved, so the skim needs no code anywhere else.
    void Deposit(const FactionId& faction, int amount);

    // Transactional, all-or-nothing -- ported from legacy StarReach2's SpendFactionStock
    // contract. Returns false and changes nothing if the faction cannot afford `amount`.
    // amount <= 0 always succeeds without touching the stock.
    bool Spend(const FactionId& faction, int amount);

    // A royalty stream from a sold Template (architecture.md 12.7): every future Deposit() to
    // `payer` pays `rate` of the deposited amount to `recipient` first. Replaces any existing
    // stream for the same (payer, templateId) pair -- a re-pitch of the same Template to the
    // same faction updates the rate rather than stacking a second skim.
    void AddRoyalty(const FactionId& payer, const BlueprintId& templateId,
                    const FactionId& recipient, float rate);

    // No-op if no stream exists for this (payer, templateId) pair.
    void RemoveRoyalty(const FactionId& payer, const BlueprintId& templateId);

    // 0.0 if no stream exists for this (payer, templateId) pair. Test/inspection support.
    float RoyaltyRate(const FactionId& payer, const BlueprintId& templateId) const;

    // Cumulative lifetime deposits for `faction`, never reduced by Spend() -- features.md 5.1's
    // Economic Footprint pillar ("any active production") reads this rather than Stock(),
    // architecture.md 12.3's settled resolution: a faction that earns and immediately spends
    // everything still has a footprint, but Stock() alone would read it as already collapsed. A
    // placeholder interpretation of "production" as lifetime-cumulative rather than a per-tick
    // rate -- FactionEconomySystem has no per-tick production/expense breakdown yet to derive a
    // truer rate from. Tracks the gross amount Deposit() was called with, before any royalty
    // skim -- a faction's production footprint, not what it nets after paying royalties owed.
    int TotalProduction(const FactionId& faction) const;

private:
    struct RoyaltyStream {
        BlueprintId templateId;
        FactionId recipient;
        float rate = 0.0f;
    };

    std::unordered_map<FactionId, int> stock_;
    std::unordered_map<FactionId, std::vector<RoyaltyStream>> royalties_;
    std::unordered_map<FactionId, int> totalProduction_;
};

}  // namespace sr::core::economy
