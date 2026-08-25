#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <vector>

#include "shared/blueprints/Ids.h"
#include "shared/components/StationServices.h"
#include "shared/ui/Row.h"

// modes/space/ui/StorageScreen -- architecture.md 12.30.3's Storage half, split out of
// StationServicesMenu: "one hold, two questions" -- Deposit (vessel -> station) and Withdraw
// (station -> vessel), free of charge, offered only within one owner. The Market half (Buy/Sell,
// crossing an ownership boundary at a price) ships in P6-08 once Pricing.h/ItemId/ctx.diplomacy
// exist; a warehouse that does not deal is a complete screen on its own.
//
// Gated on the host carrying a CargoHold and no hardpoint at all (architecture.md 12.30's split),
// so unlike Bay/Repair/etc. there is no facility hardpoint for PlayerLocation to name. Under
// architecture.md 12.30's full-screen frame -- settled here, resolving that section's own open
// question -- Storage IS a real router tab, exclusively shown like every other screen; it just
// tracks its selection on bridge_view's IsStorageSelected singleton instead of by moving
// PlayerLocation, since it has no hardpoint to move onto. It is never drawn "alongside" a
// hardpoint screen any more -- the frame has room for exactly one full-screen tab at a time.
//
// modes/*/ui/ must not include systems/ (section 2.3); this builds TransferItemRequest
// (shared/components/StationServices.h) for the caller to place on the requester -- the vessel
// root the player arrived in, never PlayerControlled, which while docked is the station itself
// (architecture.md 12.30.1/12.30.3's named trap) -- and never calls
// modes/space/systems/StationServicesSystem directly.
namespace sr::space::ui::storage_screen {

// One row of either hold: the underlying stack plus whether a full-stack transfer of it would fit
// in the OTHER hold right now (mass only -- the coarse HOLD FULL case; the finer-grained NO FREE
// SLOT distinction is left to the system's own refusal). Pure -- no raylib -- so unit-testable.
struct StorageRow {
    ItemKind kind = ItemKind::Element;
    std::string id;
    int quantity = 0;
    float unitMass = 0.0f;
    bool fits = true;
    sr::ui::Row row;
};

// `rigRoot`'s living-bay contents (shared/rig/CargoView.h's Merged), each annotated with whether
// moving the whole stack to `destination` would fit by mass alone.
std::vector<StorageRow> Rows(const entt::registry& registry, entt::entity rigRoot,
                             entt::entity destination);

// `targetHold` names the destination-side CargoHold hardpoint the sibling selector has chosen
// (entt::entity, may be entt::null to keep the pre-P4-11 auto-routed placement) -- forwarded
// verbatim onto TransferItemRequest::targetHold for StationServicesSystem to act on.
TransferItemRequest BuildDepositRequest(const StorageRow& row, entt::entity targetHold);
TransferItemRequest BuildWithdrawRequest(const StorageRow& row, entt::entity targetHold);

// Every living CargoHold hardpoint under `rigRoot`'s Rig, in Rig::children order -- the sibling
// selector's pill list for one side of this screen (architecture.md 12.30.3's "Sibling holds"
// follow-on, generalising §12.30.2's TabStrip pattern: "the sibling selector generalises to
// every tab; Docking is simply where it was noticed first"). Mirrors EngineeringScreen.h's
// SiblingBenches / BayView.h's SiblingBays with a CargoHold gate in place of a FacilityKind.
std::vector<entt::entity> SiblingHolds(const entt::registry& registry, entt::entity rigRoot);

// `stored` when it is still one of `siblings`; otherwise `siblings.front()` -- the default pick
// when nothing has been explicitly clicked yet, or the previous pick died or moved out of view.
// entt::null if `siblings` is empty. Pure -- the fallback rule Update/Draw share.
entt::entity ResolveSelectedHold(const std::vector<entt::entity>& siblings, entt::entity stored);

// The station carrying a CargoHold that Deposit/Withdraw currently apply to: DockedStation's
// result, gated on a living CargoHold-carrying child and FactionRef == playerFaction
// (architecture.md 12.30.3's ownership table -- Deposit/Withdraw is the "Yours" row's warehouse
// pair). entt::null otherwise, including "not yours" -- the screen simply does not appear, matching
// the table's "None" outcome for every case this issue ships.
entt::entity ActiveStation(const entt::registry& registry, const FactionId& playerFaction);

// The player's own vessel (FactionRef == playerFaction) currently Docked at `station`, or
// entt::null if none. At most one exists today -- parking a second hull is not yet a shippable
// path (architecture.md 12.30.2's parked-hull gap, blocked on RigState/P10-01).
entt::entity OwnedVesselAt(const entt::registry& registry, entt::entity station,
                           const FactionId& playerFaction);

// Reads this frame's input and, while bridge_view::IsStorageSelected and ActiveStation both
// resolve, hit-tests each side's sibling strip (a click there just changes that side's chosen
// destination hold) and both ListViews -- a click on a row performs a whole-stack transfer in
// that row's direction, into whichever hold the OTHER side's strip currently has selected. No-op
// otherwise, or with no owned vessel currently docked there (OwnedVesselAt).
void Update(entt::registry& registry, const FactionId& playerFaction);

// Draws the Storage screen full-screen (architecture.md 12.30's frame; bridge_view::Draw already
// drew the one bezel around the whole window, so this does not draw its own): header (station
// name, both holds' mass used/capacity), each side's sibling strip when it has more than one
// living hold, your hold on the left, the station's on the right. No-op unless
// bridge_view::IsStorageSelected and ActiveStation both resolve.
void Draw(const entt::registry& registry, const FactionId& playerFaction);

}  // namespace sr::space::ui::storage_screen
