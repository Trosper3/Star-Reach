#pragma once

#include <raylib.h>

#include <entt/entity/registry.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/economy/FactionEconomy.h"
#include "shared/blueprints/Ids.h"
#include "shared/blueprints/Taxonomy.h"
#include "shared/ui/Fonts.h"

// modes/space/ui/ -- BridgeView, the docked-menu router (architecture.md section 12.24 step 5,
// section 12.30). features.md section 4's "component-driven menus": "the Bridge UI generates
// from physical modules... destroying a hardpoint removes its tab mid-session".
//
// features.md section 4 as a whole describes a much larger RTS fleet-command mode (macro-
// commands, AI directive autonomy) that is explicitly 📋 with open design questions still
// unresolved in the design doc itself -- not buildable yet, and not what this issue scopes.
// What IS buildable now is the piece that quote actually describes: which tabs exist is a
// direct, live function of which FacilityRef hardpoints the docked station has and has not lost
// (shared/components/Facility.h, wired in RigFactory.cpp's AttachModule), plus a Storage tab
// keyed on the host simply carrying a CargoHold (architecture.md 12.30.3 -- no facility
// hardpoint at all). Five of the seven tabs (Bay, Storage, Repair, Engineering, Research) now
// have shipped screen content behind them (architecture.md 12.30.2-.8); Market and Manufacturing
// are still gated off by IsScreenShipped in BridgeView.cpp. This file itself is the tab list,
// selection, and the PlayerLocation write selecting one performs.
namespace sr::space::ui::bridge_view {

// One entry in the router's tab strip. Every screen but Storage is exactly one FacilityKind
// (architecture.md 12.30's screen inventory); Storage has no facility hardpoint at all, so
// `hardpoint` is entt::null for it -- there is nothing to move PlayerLocation onto, and its
// per-screen readout measures the station's aggregate integrity instead of one hardpoint's.
enum class ScreenId : std::uint8_t {
    Bay,            // FacilityKind::Docking
    Market,         // FacilityKind::Trade
    Storage,        // No FacilityKind -- gated on the host carrying a CargoHold (12.30.3)
    Repair,         // FacilityKind::Repair
    Engineering,    // FacilityKind::Engineering
    Manufacturing,  // FacilityKind::Manufacturing
    Research,       // FacilityKind::Research
};

std::string_view ToString(ScreenId value);

// "aegis_directorate" -> "AEGIS DIRECTORATE" -- the one FactionId->display-text rule every
// faction id in data/base_game/ already happens to read correctly under (RelationSeeding.cpp's
// seeded ids are themselves snake_case words), so no separate display-name registry exists yet.
// Exported for BayView's roster subtitles ("Trade Guild -- not yours") as well as this file's own
// ownership badge -- ui-to-ui reuse within modes/space/ui/ is already established (BayView already
// depends on IsStorageSelected/FrameContentRect below).
std::string FactionDisplayName(const FactionId& faction);

struct BridgeTab {
    ScreenId screen = ScreenId::Bay;
    // The hardpoint selecting this tab moves PlayerLocation onto (architecture.md 12.30's tab-
    // list fix). entt::null for Storage -- see ScreenId's comment above.
    entt::entity hardpoint = entt::null;
};

// The mandatory per-screen facility-health readout (features.md 3.4), read out of whichever
// screen is currently active and drawn once by the router's top bar (issue #225's visual-chrome
// pass) rather than by each screen's own header -- the same "drawn once by the router" argument
// this file's Draw() already makes for the edge channel. `label` names the specific facility
// ("BAY ALPHA"); `fraction` is its health, already clamped by DrawGauge. Each screen that wants a
// top-bar readout exports its own query returning this (see BayView.h's ActiveGaugeStatus); the
// caller (SpaceFlight.cpp) is the one place that may know about every screen, so it resolves which
// query applies and passes the result in here rather than this file depending on bay_view et al.
struct GaugeStatus {
    std::string label;
    float fraction = 1.0f;
};

// Distinct screens available among `rigRoot`'s live (non-Destroyed) hardpoints, in ScreenId
// declaration order -- first living hardpoint of each FacilityKind, deduped. Also gated on a
// compile-time shipped-table (architecture.md 12.30: "a tab needs a working screen behind it, not
// just a living hardpoint") -- a living Trade or Manufacturing hardpoint does not yet produce a
// Market or Manufacturing tab. Pure -- no raylib -- so unit-testable. Empty for a rig with no
// Facility hardpoints and no CargoHold, and for entt::null.
std::vector<BridgeTab> AvailableTabs(const entt::registry& registry, entt::entity rigRoot);

// The station rig root the player is currently docked at, found via PlayerLocation rather than
// the derived PlayerControlled -- so this keeps resolving once a tab moves PlayerLocation off the
// player's own vessel and onto one of the station's facility hardpoints (architecture.md 12.30.1:
// PlayerControlled there is the station itself, which never carries Docked, only a visiting
// vessel does). entt::null while flying, or once undocked. Shared by every docked screen that
// needs "which station is this" without depending on a specific FacilityKind hardpoint --
// architecture.md 12.30.3's Storage half is the first with no hardpoint of its own to anchor on.
entt::entity DockedStation(const entt::registry& registry);

// Pure -- the tab-selection half of the router (architecture.md 12.30.1: "selecting a tab is
// moving into that hardpoint"). Moves PlayerLocation from `shell` onto `tabs[tabIndex].hardpoint`
// and nothing else; PlayerControlled is derived elsewhere (modes/space/systems/
// PlayerLocationSystem.h), never written here. No-op when `tabIndex` is out of range, when that
// tab's hardpoint is entt::null (Storage -- nothing physical to stand in), or when it already
// names `shell`.
void SelectTab(entt::registry& registry, entt::entity shell, std::span<const BridgeTab> tabs,
               int tabIndex);

// Reads this frame's input (raylib) and the PlayerControlled entity's Docked state: hit-tests the
// tab strip Draw() below renders and, on a click, calls SelectTab. No-op with no PlayerControlled
// entity, or one that is not Docked. Also clears IsStorageSelected the moment DockedStation goes
// null (undocking), so the next docking -- this station or another -- always opens on Bay rather
// than carrying a stale Storage selection forward.
void Update(entt::registry& registry);

// Draws the docked frame: architecture.md 12.30's full-window bezel (sr::ui::DrawPanelFrame over
// the whole screen, drawn exactly once here rather than once per screen -- "the edge channel is
// drawn by the router... never by the screens, which would be seven copies of it," extended to
// the frame itself), a top bar (station name, an ownership badge naming the docked station's own
// faction, `economy`'s credit balance for `playerFaction`, and `activeGauge` when the active
// screen has one), and a left-hand vertical icon rail in place of the old horizontal tab strip
// (issue #225's visual-chrome pass, matching the Docking Screens Redesign reference). `fonts` is
// shared/ui/Fonts.h's Orbitron/Exo2 pair -- the less-pixelated follow-up to the visual-chrome pass,
// replacing raylib's built-in bitmap font everywhere this router draws text. Only when the
// PlayerControlled entity is currently Docked. No-op otherwise.
void Draw(const entt::registry& registry, const FactionId& playerFaction,
          const core::economy::FactionEconomy& economy, const sr::ui::Fonts& fonts,
          const std::optional<GaugeStatus>& activeGauge);

// architecture.md 12.30's frame: the content Rectangle every docked screen lays its own sections
// into -- below the top bar and to the right of the icon rail Draw() above renders. Pure --
// GetScreenWidth/Height only, no registry -- so every screen can call it without depending on
// bridge_view's registry-reading half. Screens must not call sr::ui::DrawPanelFrame on their own
// bounds any more: Draw() already drew the one bezel for the whole window, and a second bezel
// nested inside it would double the chrome for no reason.
Rectangle FrameContentRect();

// Tag singleton: true exactly while Storage is the explicitly-clicked router tab.
//
// architecture.md 12.30's own open question -- "does Storage become its own full-screen tab, or
// does the frame make an explicit exception for a docked-only overlay layered on top?" -- is
// resolved here: Storage is a real, exclusively-shown tab like every other screen. It has no
// FacilityKind hardpoint for PlayerLocation to name (architecture.md 12.30.3), so SelectTab
// tracks its selection on this singleton instead of by moving PlayerLocation; every hardpoint
// screen (Bay/Repair/Engineering/Research) checks it too; alongside its own PlayerLocation gate,
// so at most one screen is ever shown full-screen at a time. Selecting any other tab clears it
// (SelectTab); undocking clears it too (Update).
bool IsStorageSelected(const entt::registry& registry);

}  // namespace sr::space::ui::bridge_view
