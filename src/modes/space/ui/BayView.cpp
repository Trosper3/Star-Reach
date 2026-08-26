#include "modes/space/ui/BayView.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>

#include "modes/space/ui/BridgeView.h"
#include "modes/space/ui/CockpitHud.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/ui/Fonts.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::bay_view {
namespace {

constexpr float kHeaderHeight = 54.0f;
constexpr float kSiblingStripHeight = 28.0f;
constexpr float kRosterLabelHeight = 22.0f;
constexpr float kRosterRowHeight = 70.0f;
constexpr float kRosterVisibleRows = 4.0f;
constexpr float kRosterListHeight = kRosterRowHeight * kRosterVisibleRows;
constexpr float kSectionGap = 10.0f;
constexpr float kIconBoxSize = 44.0f;
constexpr float kButtonWidth = 96.0f;
constexpr float kButtonHeight = 32.0f;

// Reference: the "Bay" artboard on the Docking Screens Redesign canvas (issue #224) names sibling
// bays ALPHA/BRAVO/... rather than the bare ordinal BayView drew before -- purely a display label,
// the underlying index into `siblings` is still what SelectTab/TabStripHitTest key on.
constexpr std::array<const char*, 10> kPhoneticNames = {
    "ALPHA", "BRAVO", "CHARLIE", "DELTA", "ECHO", "FOXTROT", "GOLF", "HOTEL", "INDIA", "JULIET"};

std::string BayLabel(std::size_t index) {
    if (index < kPhoneticNames.size()) {
        return std::string("BAY ") + kPhoneticNames[index];
    }
    return "BAY " + std::to_string(index + 1);
}

// features.md 3.9's status triad, the same three-stop thresholds BayView's gauge and RepairScreen's
// gauge both already used -- pulled out so the header stat line and the (now gauge-free) integrity
// readout agree on one rule.
Color IntegrityStatusColor(float fraction) {
    return fraction > 0.5f   ? sr::ui::kStatusGood
           : fraction > 0.2f ? sr::ui::kStatusCaution
                             : sr::ui::kStatusCritical;
}

// 1.0 (undamaged) for a hardpoint with no Health at all, or one whose max is zero -- the same
// "missing reads as full" default every other screen's integrity readout already uses.
float HardpointIntegrityFraction(const entt::registry& registry, entt::entity hardpoint) {
    const Health* health = registry.try_get<Health>(hardpoint);
    return health != nullptr && health->max > 0.0f ? health->current / health->max : 1.0f;
}

// The entity PlayerLocation currently names, or entt::null. Exactly one per registry
// (architecture.md 12.30.1).
entt::entity PlayerShell(const entt::registry& registry) {
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        return entity;
    }
    return entt::null;
}

// The living Docking-kind facility hardpoint PlayerLocation currently names ("this bay"), or
// entt::null when the player is not currently viewing the Bay screen -- flying, docked but
// standing on a different facility hardpoint, aboard a vessel rather than inside the station, or
// with Storage currently the explicitly-selected tab (bridge_view::IsStorageSelected):
// architecture.md 12.30's frame leaves room for exactly one full-screen tab at a time, and Bay's
// own PlayerLocation-based gate can't see that Storage has no hardpoint of its own to contest it
// with.
entt::entity CurrentBay(const entt::registry& registry, entt::entity shell) {
    if (shell == entt::null || registry.all_of<Destroyed>(shell)) {
        return entt::null;
    }
    const FacilityRef* facility = registry.try_get<FacilityRef>(shell);
    if (facility == nullptr || facility->kind != FacilityKind::Docking) {
        return entt::null;
    }
    if (bridge_view::IsStorageSelected(registry)) {
        return entt::null;
    }
    return shell;
}

struct Layout {
    Rectangle content{};
    Rectangle header{};
    Rectangle siblingStrip{};  // Zero height when there is only one bay.
    Rectangle rosterPanel{};   // The bracket-bordered box wrapping rosterLabel + roster.
    Rectangle rosterLabel{};   // "ROSTER -- BAY ALPHA" / slot count, inside rosterPanel.
    Rectangle roster{};        // The ListView content rect -- what Update() hit-tests against.
};

// `content` is bridge_view::FrameContentRect() -- already inset by the router's one bezel, so this
// lays sections out inside it directly rather than re-insetting via sr::ui::PanelContentRect,
// except for rosterPanel's own interior, which gets exactly one nested inset (the roster is the
// one section framed as its own sub-panel, per issue #224's reference).
Layout ComputeLayout(Rectangle content, bool showSiblingStrip) {
    Layout layout;
    layout.content = content;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    float y = content.y + kHeaderHeight + kSectionGap;
    if (showSiblingStrip) {
        layout.siblingStrip = {content.x, y, content.width, kSiblingStripHeight};
        y += kSiblingStripHeight + kSectionGap;
    }

    const float panelHeight = kRosterLabelHeight + kRosterListHeight + sr::ui::kPanelPadding * 2.0f;
    layout.rosterPanel = {content.x, y, content.width, panelHeight};
    const Rectangle rosterContent = sr::ui::PanelContentRect(layout.rosterPanel);
    layout.rosterLabel = {rosterContent.x, rosterContent.y, rosterContent.width,
                          kRosterLabelHeight};
    layout.roster = {rosterContent.x, rosterContent.y + kRosterLabelHeight, rosterContent.width,
                     kRosterListHeight};
    return layout;
}

// architecture.md 2.2's function-length cap -- split out of Draw() below, one section each.

void DrawHeader(Rectangle header, const sr::ui::Fonts& fonts, const std::string& slots,
                int occupied, float integrityFraction) {
    // "DOCKING BAY" -- a fixed screen title rather than the station's own DisplayName: the
    // router's top bar (bridge_view::Draw, issue #225's visual-chrome pass) already names the
    // specific station under "DOCKED AT", so repeating it here doubled the same fact.
    DrawTextEx(fonts.heading, "DOCKING BAY", {header.x, header.y}, 24.0f, 1.0f,
               sr::ui::kValueBright);

    const float statY = header.y + 30.0f;
    float statX = header.x;
    const std::string statPrefix =
        slots + " | " + std::to_string(occupied) + " OCCUPIED | INTEGRITY ";
    DrawTextEx(fonts.body, statPrefix.c_str(), {statX, statY}, 14.0f, 1.0f, sr::ui::kLabelDim);
    statX += MeasureTextEx(fonts.body, statPrefix.c_str(), 14.0f, 1.0f).x;
    const std::string integrityPct = std::to_string(std::lround(integrityFraction * 100.0f)) + "%";
    DrawTextEx(fonts.body, integrityPct.c_str(), {statX, statY}, 14.0f, 1.0f,
               IntegrityStatusColor(integrityFraction));
}

void DrawSiblingStrip(Rectangle bounds, const sr::ui::Fonts& fonts,
                      const std::vector<entt::entity>& siblings, entt::entity thisBay) {
    std::vector<std::string> labels;
    labels.reserve(siblings.size());
    int selected = -1;
    for (std::size_t i = 0; i < siblings.size(); ++i) {
        labels.push_back(BayLabel(i));
        if (siblings[i] == thisBay) {
            selected = static_cast<int>(i);
        }
    }
    sr::ui::DrawTabStrip(bounds, labels, selected, fonts.body);
}

// The reference's "ROSTER -- BAY ALPHA" box: a bracket panel plus a label row naming the bay and
// its slot count, and a divider rule separating the label from the rows below it -- the mock's
// own hairline under every panel header, missing here until now.
void DrawRosterPanel(const Layout& layout, const sr::ui::Fonts& fonts, std::size_t thisBayIndex,
                     const std::string& slots) {
    sr::ui::DrawBracketPanel(layout.rosterPanel, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f,
                             2.0f);
    const std::string rosterTitle = "ROSTER -- " + BayLabel(thisBayIndex);
    DrawTextEx(fonts.body, rosterTitle.c_str(), {layout.rosterLabel.x, layout.rosterLabel.y}, 14.0f,
               1.0f, sr::ui::kValueBright);
    const float slotsWidth = MeasureTextEx(fonts.body, slots.c_str(), 14.0f, 1.0f).x;
    DrawTextEx(fonts.body, slots.c_str(),
               {layout.rosterLabel.x + layout.rosterLabel.width - slotsWidth, layout.rosterLabel.y},
               14.0f, 1.0f, sr::ui::kLabelDim);
    const float dividerY = layout.rosterLabel.y + layout.rosterLabel.height;
    DrawLineEx({layout.rosterLabel.x, dividerY},
               {layout.rosterLabel.x + layout.rosterLabel.width, dividerY}, 1.0f, sr::ui::kDivider);
}

// A generic "vessel" pictograph (an upward triangle, matching the reference for every ship
// regardless of blueprint -- identity here is "this is a ship," not which one) inside a bordered
// box. Border colour carries condition (features.md 3.9): disabled dims it to kLabelDim, otherwise
// it is the vessel's own hull-integrity colour, never both a monogram letter and a status colour
// competing for the same glyph the way BlueprintGlyph used to.
void DrawRosterIcon(Rectangle box, bool disabled, float integrityFraction) {
    const Color color = disabled ? sr::ui::kLabelDim : IntegrityStatusColor(integrityFraction);
    DrawRectangleLinesEx(box, disabled ? 1.0f : 1.5f, color);
    const float cx = box.x + box.width / 2.0f;
    const float cy = box.y + box.height / 2.0f;
    DrawTriangle({cx - 6.0f, cy + 7.0f}, {cx + 6.0f, cy + 7.0f}, {cx, cy - 8.0f},
                 Color{0, 0, 0, 255});
}

// One roster row, card-style (issue #225's visual-chrome pass): the icon box, a bold vessel name
// over a dim subtitle, and -- only for a row with a verb to offer -- a real bordered button
// instead of `row.value` as plain inline text. LAUNCH reads kStatusGood (the affirmative, already-
// yours-and-occupied action); BOARD reads kPanelChrome (a neutral, still-available action); a
// foreign vessel gets neither, matching "not yours -- no actions available."
void DrawRosterRow(Rectangle bounds, const sr::ui::Fonts& fonts, const BayRosterEntry& entry) {
    const Rectangle iconBox{bounds.x, bounds.y + (bounds.height - kIconBoxSize) / 2.0f,
                            kIconBoxSize, kIconBoxSize};
    DrawRosterIcon(iconBox, entry.row.style.disabled, entry.row.style.integrity);

    const float textX = iconBox.x + iconBox.width + 14.0f;
    const Color nameColor = entry.row.style.disabled ? sr::ui::kLabelDim : sr::ui::kValueBright;
    DrawTextEx(fonts.heading, entry.row.label.c_str(),
               {textX, bounds.y + bounds.height / 2.0f - 16.0f}, 16.0f, 1.0f, nameColor);
    DrawTextEx(fonts.body, entry.row.subtitle.c_str(),
               {textX, bounds.y + bounds.height / 2.0f + 4.0f}, 12.0f, 1.0f, sr::ui::kLabelDim);

    if (entry.row.value.empty()) {
        return;
    }
    const Rectangle button{bounds.x + bounds.width - kButtonWidth,
                           bounds.y + (bounds.height - kButtonHeight) / 2.0f, kButtonWidth,
                           kButtonHeight};
    const Color accent = entry.occupied ? sr::ui::kStatusGood : sr::ui::kPanelChrome;
    if (entry.occupied) {
        const float tagWidth = MeasureTextEx(fonts.body, "YOU", 11.0f, 1.0f).x + 16.0f;
        const Rectangle youTag{button.x - tagWidth - 8.0f,
                               button.y + (button.height - 22.0f) / 2.0f, tagWidth, 22.0f};
        DrawRectangleLinesEx(youTag, 1.0f, sr::ui::kLabelDim);
        DrawTextEx(fonts.body, "YOU", {youTag.x + 8.0f, youTag.y + 5.0f}, 11.0f, 1.0f,
                   sr::ui::kLabelDim);
    }
    DrawRectangleLinesEx(button, 1.5f, accent);
    const float labelWidth = MeasureTextEx(fonts.body, entry.row.value.c_str(), 14.0f, 1.0f).x;
    DrawTextEx(
        fonts.body, entry.row.value.c_str(),
        {button.x + (button.width - labelWidth) / 2.0f, button.y + (button.height - 14.0f) / 2.0f},
        14.0f, 1.0f, accent);
}

// The roster's card rows, top to bottom inside `bounds`, divider rules between them -- the
// generic sr::ui::ListView draws one flat text line per row and has no icon box, subtitle, or
// button of its own, so the roster (issue #225's reference) draws its own rather than stretch
// that widget to a shape only this screen needs (Law 11: one consumer so far).
void DrawRosterRows(Rectangle bounds, const sr::ui::Fonts& fonts,
                    const std::vector<BayRosterEntry>& roster) {
    BeginScissorMode(static_cast<int>(bounds.x), static_cast<int>(bounds.y),
                     static_cast<int>(bounds.width), static_cast<int>(bounds.height));
    if (roster.empty()) {
        DrawTextEx(fonts.body, "BAY EMPTY", {bounds.x, bounds.y}, 14.0f, 1.0f, sr::ui::kLabelDim);
        EndScissorMode();
        return;
    }
    for (std::size_t i = 0; i < roster.size(); ++i) {
        const float y = bounds.y + static_cast<float>(i) * kRosterRowHeight;
        if (y > bounds.y + bounds.height) {
            break;
        }
        if (i > 0) {
            DrawLineEx({bounds.x, y}, {bounds.x + bounds.width, y}, 1.0f, sr::ui::kDivider);
        }
        DrawRosterRow({bounds.x, y, bounds.width, kRosterRowHeight}, fonts, roster[i]);
    }
    EndScissorMode();
}

// Pure -- the card-row analog of sr::ui::ListViewRowAt, using kRosterRowHeight instead of
// sr::ui::kListRowHeight. No scroll offset: the roster panel is sized for kRosterVisibleRows
// (matching the reference), and scrolling past that is out of this pass's scope.
std::optional<int> RosterRowAt(Rectangle bounds, int rowCount, Vector2 cursor) {
    if (rowCount <= 0 || !CheckCollisionPointRec(cursor, bounds)) {
        return std::nullopt;
    }
    const int index = static_cast<int>((cursor.y - bounds.y) / kRosterRowHeight);
    if (index < 0 || index >= rowCount) {
        return std::nullopt;
    }
    return index;
}

}  // namespace

std::vector<BayRosterEntry> Roster(const entt::registry& registry, entt::entity bay,
                                   entt::entity occupiedVessel, const FactionId& playerFaction) {
    std::vector<BayRosterEntry> entries;
    for (auto [vessel, docked, faction] : registry.view<Docked, FactionRef>().each()) {
        if (docked.bay != bay) {
            continue;
        }
        BayRosterEntry entry;
        entry.vessel = vessel;
        entry.owned = faction.id == playerFaction;
        entry.occupied = vessel == occupiedVessel;

        if (const DisplayName* name = registry.try_get<DisplayName>(vessel)) {
            entry.row.label = name->value;
        }
        entry.row.style.integrity = cockpit_hud::AggregateHullFraction(registry, vessel);
        // Not owned -> disabled (features.md 3.10's degrade-never-remove greyed-out state): a
        // foreign vessel is shown, never hidden, but never tinted by its own hull health either,
        // since there is nothing the player can do about it either way.
        entry.row.style.disabled = !entry.owned;
        if (entry.occupied) {
            entry.row.value = "LAUNCH";
            entry.row.subtitle = "Your vessel -- currently occupied";
        } else if (entry.owned) {
            entry.row.value = "BOARD";
            entry.row.subtitle = "Your vessel -- docked, unoccupied";
        } else {
            entry.row.subtitle = bridge_view::FactionDisplayName(faction.id) +
                                 " -- not yours -- no actions available";
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::vector<entt::entity> SiblingBays(const entt::registry& registry, entt::entity station) {
    std::vector<entt::entity> bays;
    const Rig* rig = registry.try_get<Rig>(station);
    if (rig == nullptr) {
        return bays;
    }
    for (const entt::entity child : rig->children) {
        if (registry.all_of<Destroyed>(child)) {
            continue;
        }
        const FacilityRef* facility = registry.try_get<FacilityRef>(child);
        if (facility != nullptr && facility->kind == FacilityKind::Docking) {
            bays.push_back(child);
        }
    }
    return bays;
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

void Board(entt::registry& registry, entt::entity vessel) {
    const entt::entity shell = PlayerShell(registry);
    if (shell == entt::null || shell == vessel) {
        return;
    }
    registry.remove<PlayerLocation>(shell);
    registry.emplace<PlayerLocation>(vessel, PlayerLocation{vessel});
}

void Launch(entt::registry& registry, entt::entity vessel) {
    // Board first: while standing on the bay hardpoint (not aboard `vessel`), PlayerLocation
    // never moves on its own, so firing UndockRequest alone strands it on the hardpoint --
    // DockingSystem's undock path clears Docked but never touches PlayerLocation. Mirrors
    // AvionicsMenu's "R = board and launch" shortcut, found missing by #207's meso-loop trace.
    Board(registry, vessel);
    registry.emplace_or_replace<UndockRequest>(vessel);
}

void Update(entt::registry& registry, const FactionId& playerFaction) {
    const entt::entity shell = PlayerShell(registry);
    const entt::entity thisBay = CurrentBay(registry, shell);
    if (thisBay == entt::null) {
        return;
    }
    const entt::entity station = registry.get<ParentRig>(thisBay).root;
    // Not registry.view<PlayerControlled>() -- while PlayerLocation names any facility hardpoint,
    // including this bay, the derived PlayerControlled IS the station (architecture.md 12.30.1),
    // never the docked vessel, so that view can never equal a roster row's `vessel`. OwnedVesselAt
    // is what every other docked screen already uses to resolve "the hull the player arrived in."
    const entt::entity occupiedVessel = OwnedVesselAt(registry, station, playerFaction);

    const sr::ui::UiInput input{GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                GetMouseWheelMove()};
    if (!input.clicked) {
        return;
    }

    const std::vector<entt::entity> siblings = SiblingBays(registry, station);
    const Layout layout = ComputeLayout(bridge_view::FrameContentRect(), siblings.size() > 1);

    if (siblings.size() > 1) {
        const std::optional<int> hit = sr::ui::TabStripHitTest(
            layout.siblingStrip, static_cast<int>(siblings.size()), input.cursor);
        if (hit.has_value()) {
            const entt::entity target = siblings[static_cast<std::size_t>(*hit)];
            if (target != thisBay) {
                registry.remove<PlayerLocation>(thisBay);
                registry.emplace<PlayerLocation>(target, PlayerLocation{target});
            }
            return;
        }
    }

    const std::vector<BayRosterEntry> roster =
        Roster(registry, thisBay, occupiedVessel, playerFaction);
    const std::optional<int> hit =
        RosterRowAt(layout.roster, static_cast<int>(roster.size()), input.cursor);
    if (!hit.has_value() || *hit >= static_cast<int>(roster.size())) {
        return;
    }
    const BayRosterEntry& entry = roster[static_cast<std::size_t>(*hit)];
    if (entry.occupied) {
        Launch(registry, entry.vessel);
    } else if (entry.owned) {
        Board(registry, entry.vessel);
    }
}

void Draw(const entt::registry& registry, const FactionId& playerFaction,
          const sr::ui::Fonts& fonts) {
    const entt::entity shell = PlayerShell(registry);
    const entt::entity thisBay = CurrentBay(registry, shell);
    if (thisBay == entt::null) {
        return;
    }
    const entt::entity station = registry.get<ParentRig>(thisBay).root;
    // See the matching comment in Update() above -- PlayerControlled is the station here, not the
    // vessel.
    const entt::entity occupiedVessel = OwnedVesselAt(registry, station, playerFaction);

    const std::vector<entt::entity> siblings = SiblingBays(registry, station);
    const bool showSiblingStrip = siblings.size() > 1;
    const Layout layout = ComputeLayout(bridge_view::FrameContentRect(), showSiblingStrip);

    // Header: a fixed "DOCKING BAY" title over one consolidated stat line -- slots, occupancy,
    // and this bay hardpoint's integrity (features.md 3.4's mandatory per-screen health readout,
    // also fed to the router's top-bar Gauge via ActiveGaugeStatus below) -- rather than a
    // name/occupancy pair of lines plus a half-width gauge bar. Closer to issue #224/#225's
    // reference, which reads the three facts as one line under the title.
    int occupied = 0;
    for (auto [vessel, docked] : registry.view<Docked>().each()) {
        (void)vessel;
        if (docked.bay == thisBay) {
            ++occupied;
        }
    }
    const FacilityRef* facility = registry.try_get<FacilityRef>(thisBay);
    const int capacity = facility != nullptr ? facility->capacity : 0;
    const std::string slots =
        capacity == 0 ? "UNLIMITED SLOTS" : (std::to_string(capacity) + " SLOTS");

    const float integrityFraction = HardpointIntegrityFraction(registry, thisBay);
    DrawHeader(layout.header, fonts, slots, occupied, integrityFraction);

    // Sibling selector: one TabStrip entry per living Docking hardpoint on the host, absent when
    // there is only one bay. Labelled phonetically (BAY ALPHA/BRAVO/...) to match the reference,
    // rather than the bare ordinal this screen drew before.
    if (showSiblingStrip) {
        DrawSiblingStrip(layout.siblingStrip, fonts, siblings, thisBay);
    }

    // Roster: view<Docked> filtered on docked.bay == thisBay, framed as its own bracket-bordered
    // sub-panel (DrawRosterPanel above). Your own vessel is a row like any other, marked via
    // `value` ("LAUNCH" if you occupy it, "BOARD" if you own it and don't) -- unchanged from
    // before, out of this issue's scope.
    const auto siblingIt = std::find(siblings.begin(), siblings.end(), thisBay);
    const std::size_t thisBayIndex =
        siblingIt != siblings.end() ? static_cast<std::size_t>(siblingIt - siblings.begin()) : 0;
    DrawRosterPanel(layout, fonts, thisBayIndex, slots);

    const std::vector<BayRosterEntry> roster =
        Roster(registry, thisBay, occupiedVessel, playerFaction);
    DrawRosterRows(layout.roster, fonts, roster);
}

std::optional<bridge_view::GaugeStatus> ActiveGaugeStatus(const entt::registry& registry) {
    const entt::entity shell = PlayerShell(registry);
    const entt::entity thisBay = CurrentBay(registry, shell);
    if (thisBay == entt::null) {
        return std::nullopt;
    }
    const entt::entity station = registry.get<ParentRig>(thisBay).root;
    const std::vector<entt::entity> siblings = SiblingBays(registry, station);
    const auto it = std::find(siblings.begin(), siblings.end(), thisBay);
    const std::size_t index =
        it != siblings.end() ? static_cast<std::size_t>(it - siblings.begin()) : 0;
    return bridge_view::GaugeStatus{BayLabel(index), HardpointIntegrityFraction(registry, thisBay)};
}

}  // namespace sr::space::ui::bay_view
