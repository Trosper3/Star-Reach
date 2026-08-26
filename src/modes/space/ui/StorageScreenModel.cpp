// StorageScreen.cpp's own pure data half, split out to satisfy architecture.md 2.2's 600-line
// file cap (issue #225's Storage visual-chrome pass pushed the drawing half well past it on its
// own): Rows and the resolvers around it carry no raylib and no layout, so they cost nothing to
// keep in their own translation unit, unlike Update()/Draw()'s layout math which stays with the
// widgets it positions.
#include "modes/space/ui/StorageScreen.h"

#include <algorithm>
#include <string>
#include <vector>

#include "modes/space/ui/BridgeView.h"
#include "shared/components/Docking.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"

namespace sr::space::ui::storage_screen {
namespace {

bool HasCargoHold(const entt::registry& registry, entt::entity station) {
    const Rig* rig = registry.try_get<Rig>(station);
    if (rig == nullptr) {
        return false;
    }
    for (const entt::entity child : rig->children) {
        if (registry.all_of<CargoHold>(child) && !registry.all_of<Destroyed>(child)) {
            return true;
        }
    }
    return false;
}

}  // namespace

std::vector<StorageRow> Rows(const entt::registry& registry, entt::entity rigRoot,
                             entt::entity destination) {
    std::vector<StorageRow> rows;
    const float destCapacity = cargo_view::Capacity(registry, destination);
    const float destUsed = cargo_view::TotalMass(registry, destination);
    const float destRoom = destCapacity - destUsed;

    for (const ItemStack& stack : cargo_view::Merged(registry, rigRoot)) {
        StorageRow entry;
        entry.kind = stack.kind;
        entry.id = stack.id;
        entry.quantity = stack.quantity;
        entry.unitMass = stack.unitMass;
        entry.fits = destRoom >= static_cast<float>(stack.quantity) * stack.unitMass;

        entry.row.label = stack.id;
        entry.row.value = "x" + std::to_string(stack.quantity);
        entry.row.style.disabled = !entry.fits;
        if (!entry.fits) {
            entry.row.value += "  FULL";
        }
        rows.push_back(std::move(entry));
    }
    return rows;
}

TransferItemRequest BuildDepositRequest(const StorageRow& row, entt::entity targetHold) {
    TransferItemRequest request;
    request.kind = row.kind;
    request.id = row.id;
    request.quantity = row.quantity;
    request.toStation = true;
    request.targetHold = targetHold;
    return request;
}

TransferItemRequest BuildWithdrawRequest(const StorageRow& row, entt::entity targetHold) {
    TransferItemRequest request = BuildDepositRequest(row, targetHold);
    request.toStation = false;
    return request;
}

std::vector<entt::entity> SiblingHolds(const entt::registry& registry, entt::entity rigRoot) {
    std::vector<entt::entity> holds;
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return holds;
    }
    for (const entt::entity child : rig->children) {
        if (!registry.all_of<Destroyed>(child) && registry.all_of<CargoHold>(child)) {
            holds.push_back(child);
        }
    }
    return holds;
}

entt::entity ResolveSelectedHold(const std::vector<entt::entity>& siblings, entt::entity stored) {
    if (siblings.empty()) {
        return entt::null;
    }
    if (std::find(siblings.begin(), siblings.end(), stored) != siblings.end()) {
        return stored;
    }
    return siblings.front();
}

entt::entity OwnedVesselAt(const entt::registry& registry, entt::entity station,
                           const FactionId& playerFaction) {
    for (auto [vessel, docked, faction] : registry.view<Docked, FactionRef>().each()) {
        if (docked.station == station && faction.id == playerFaction) {
            return vessel;
        }
    }
    return entt::null;
}

// Structural resolver only: "is there a valid, owned CargoHold station to trade with here at
// all," independent of whether Storage is the tab currently on screen. Deliberately does not
// consult bridge_view::IsStorageSelected -- Update/Draw are what enforce the frame's one-screen-
// at-a-time exclusivity (architecture.md 12.30's Storage sub-question), so this stays the same
// pure "could Storage apply" check regardless of which tab is showing.
entt::entity ActiveStation(const entt::registry& registry, const FactionId& playerFaction) {
    const entt::entity station = bridge_view::DockedStation(registry);
    if (station == entt::null || !HasCargoHold(registry, station)) {
        return entt::null;
    }
    const FactionRef* faction = registry.try_get<FactionRef>(station);
    if (faction == nullptr || faction->id != playerFaction) {
        return entt::null;  // architecture.md 12.30.3: Deposit/Withdraw only within one owner.
    }
    return station;
}

}  // namespace sr::space::ui::storage_screen
