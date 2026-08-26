#pragma once

#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <optional>
#include <vector>

#include "core/registries/ContentLibrary.h"
#include "modes/space/ui/BridgeView.h"
#include "shared/blueprints/Ids.h"
#include "shared/ui/Fonts.h"
#include "shared/ui/Row.h"

// modes/space/ui/RepairScreen -- architecture.md 12.30.4, "Screen 3 -- Repair." Gated on a
// living FacilityKind::Repair hardpoint, so unlike Storage this uses BayView's own pattern: the
// screen is active exactly while PlayerLocation names that hardpoint.
//
// Subject: a rig you own present here -- your own vessel, or the station you are standing in when
// it is yours (architecture.md 12.30.3's ownership test, "a station with a repair bay repairs
// itself"). architecture.md 12.30.4 draws these as a TabStrip you switch between; this instead
// draws one section per valid subject (one when the station is not yours, two when it is) so no
// new retained "which subject is selected" state is needed -- the same simplification
// modes/space/ui/BayView.h's click-to-act rows already make for this issue's Bay screen.
//
// modes/*/ui/ must not include systems/ (section 2.3); this builds RepairOrder
// (shared/components/StationServices.h) for the caller to place on the requester -- the vessel
// root the player arrived in, never PlayerControlled, which while docked is the station itself
// (architecture.md 12.30.1/12.30.3's named trap) -- and never calls
// modes/space/systems/StationServicesSystem directly.
namespace sr::space::ui::repair_screen {

// One hardpoint row of one subject's Rig. Pure -- no raylib -- so unit-testable.
struct RepairRow {
    entt::entity hardpoint = entt::null;
    bool destroyed = false;
    bool ordered = false;  // The requester's current RepairOrder targets exactly this hardpoint.
    int costToFull = 0;  // 0 for a destroyed row -- REPAIR ALL's footer sums this, not `row.value`.
    sr::ui::Row row;
};

// `subject`'s hardpoints, sorted by integrity ascending (architecture.md 12.30.4: "the thing
// most likely to kill you is the first row"). `orderedHardpoint`/`hasOrder` describe the
// requester's current RepairOrder for this subject, if any -- entt::null with hasOrder true means
// a whole-rig ("Repair All") order is active, which marks every living row `ordered`.
// `costPerHp` prices each row's "cost to bring to full" display.
std::vector<RepairRow> Rows(const entt::registry& registry, entt::entity subject, bool hasOrder,
                            entt::entity orderedHardpoint, int costPerHp);

// True if a whole-rig order (RepairOrder::hardpoint == entt::null) is currently active for
// `subject` -- what the footer's "Repair All" button reads to show REPAIR vs STOP.
bool RepairAllActive(bool hasOrder, entt::entity orderedHardpoint);

// The station's living Repair-kind hardpoint's authored grade (FacilityRef::grade), or 1 if
// absent -- core/economy/Pricing.h's RepairCostPerHp divisor.
int FacilityGrade(const entt::registry& registry, entt::entity facilityHardpoint);

// `facilityHardpoint`'s authored heal rate in HP/s, crew-boosted -- the same computation
// modes/space/systems/StationServicesSystem.cpp's file-local RepairFacilityRate performs (that
// one is not exported; modes/*/ui/ must not include systems/ per section 2.3, so this is a
// deliberate, small duplicate rather than a new cross-layer dependency), read here purely for the
// header's "RATE 5.0 HP/S" display -- it does not feed ProcessRepairOrders' own billing, which
// still computes its own rate from `ctx.content` directly. 0 with no living Repair module mounted.
float FacilityRate(const entt::registry& registry, const core::ContentLibrary& content,
                   entt::entity facilityHardpoint);

// The player's own vessel (FactionRef == playerFaction) currently Docked at `station`, or
// entt::null if none. At most one exists today -- parking a second hull is not yet a shippable
// path (architecture.md 12.30.2's parked-hull gap, blocked on RigState/P10-01).
entt::entity OwnedVesselAt(const entt::registry& registry, entt::entity station,
                           const FactionId& playerFaction);

// The mandatory per-screen facility-health readout (features.md 3.4) for the Repair hardpoint
// PlayerLocation currently names, fed to the router's top-bar Gauge via SpaceFlight's
// orchestration (mirrors BayView.h's/StorageScreen.h's own ActiveGaugeStatus) rather than Repair
// drawing its own in-page gauge any more (issue #226's visual-chrome pass). nullopt unless the
// screen is active and an owned vessel is docked there (the same gate Draw() itself uses).
std::optional<bridge_view::GaugeStatus> ActiveGaugeStatus(const entt::registry& registry,
                                                          const FactionId& playerFaction);

// Reads this frame's input and, while the player stands on a living Repair hardpoint, hit-tests
// every subject section's rows and Repair All button -- a click toggles that target's order: off
// if it already exactly matches the requester's current RepairOrder, on (replacing any existing
// order, per architecture.md 12.30.4's "one order per subject; a second replaces the first")
// otherwise. No-op on a Destroyed row.
void Update(entt::registry& registry, const FactionId& playerFaction);

// Draws the Repair screen full-screen (architecture.md 12.30's frame; bridge_view::Draw already
// drew the one bezel, top bar and icon rail around the whole window, so this does not draw a
// second one at that scale, nor its own integrity gauge any more -- ActiveGaugeStatus above feeds
// that to the router's top bar instead): a fixed "REPAIR BAY" header with a GRADE/RATE/CREDITS
// stat line, over one bracket-bordered panel per valid subject laid out side by side (issue
// #226's visual-chrome pass, matching Bay's/Storage's own reference) -- a label naming the
// subject and a short hint under a divider rule, each row drawn as a bordered icon-box row
// (border colour carries condition, matching Bay's/Storage's own row treatment) rather than one
// flat ListView line, and a REPAIR ALL/STOP footer priced from RepairRow::costToFull. `content`
// resolves FacilityRate's module lookup, the same reason EngineeringScreen::Draw already takes
// it; `fonts` is shared/ui/Fonts.h's Orbitron/Exo2 pair, replacing raylib's built-in bitmap font.
// No-op unless PlayerLocation currently names a living Repair-kind facility hardpoint and an
// owned vessel is docked there.
void Draw(const entt::registry& registry, const FactionId& playerFaction,
          const core::ContentLibrary& content, const sr::ui::Fonts& fonts);

}  // namespace sr::space::ui::repair_screen
