#include "modes/space/ui/BayView.h"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cctype>
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
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::bay_view {
namespace {

constexpr float kHeaderHeight = 54.0f;
constexpr float kSiblingStripHeight = 28.0f;
constexpr float kRosterLabelHeight = 22.0f;
constexpr float kRosterListHeight = 260.0f;
constexpr float kSectionGap = 10.0f;

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

// First two uppercased, alphanumeric characters of a BlueprintId -- the same "no per-shell art
// exists yet" placeholder shape RefactorMenu.cpp's ShellGlyph uses.
void BlueprintGlyph(const BlueprintId& id, char (&out)[3]) {
    out[0] = '\0';
    out[1] = '\0';
    out[2] = '\0';
    int written = 0;
    for (const char c : id.str()) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            continue;
        }
        out[written] = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        ++written;
        if (written == 2) {
            break;
        }
    }
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

void DrawHeader(Rectangle header, const std::string& bayName, const std::string& slots,
                int occupied, float integrityFraction) {
    DrawText(bayName.c_str(), static_cast<int>(header.x), static_cast<int>(header.y), 24,
             sr::ui::kValueBright);

    const int statY = static_cast<int>(header.y + 30.0f);
    int statX = static_cast<int>(header.x);
    const std::string statPrefix =
        slots + " | " + std::to_string(occupied) + " OCCUPIED | INTEGRITY ";
    DrawText(statPrefix.c_str(), statX, statY, 14, sr::ui::kLabelDim);
    statX += MeasureText(statPrefix.c_str(), 14);
    const std::string integrityPct = std::to_string(std::lround(integrityFraction * 100.0f)) + "%";
    DrawText(integrityPct.c_str(), statX, statY, 14, IntegrityStatusColor(integrityFraction));
}

void DrawSiblingStrip(Rectangle bounds, const std::vector<entt::entity>& siblings,
                      entt::entity thisBay) {
    std::vector<std::string> labels;
    labels.reserve(siblings.size());
    int selected = -1;
    for (std::size_t i = 0; i < siblings.size(); ++i) {
        labels.push_back(BayLabel(i));
        if (siblings[i] == thisBay) {
            selected = static_cast<int>(i);
        }
    }
    sr::ui::DrawTabStrip(bounds, labels, selected);
}

// The reference's "ROSTER -- BAY ALPHA" box: a bracket panel plus a label row naming the bay and
// its slot count, drawn around (but not including) the ListView itself.
void DrawRosterPanel(const Layout& layout, std::size_t thisBayIndex, const std::string& slots) {
    sr::ui::DrawBracketPanel(layout.rosterPanel, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f,
                             2.0f);
    const std::string rosterTitle = "ROSTER -- " + BayLabel(thisBayIndex);
    DrawText(rosterTitle.c_str(), static_cast<int>(layout.rosterLabel.x),
             static_cast<int>(layout.rosterLabel.y), 14, sr::ui::kValueBright);
    const int slotsWidth = MeasureText(slots.c_str(), 14);
    DrawText(slots.c_str(),
             static_cast<int>(layout.rosterLabel.x + layout.rosterLabel.width - slotsWidth),
             static_cast<int>(layout.rosterLabel.y), 14, sr::ui::kLabelDim);
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
        if (const BlueprintRef* blueprint = registry.try_get<BlueprintRef>(vessel)) {
            BlueprintGlyph(blueprint->id, entry.row.glyph);
        }
        entry.row.style.integrity = cockpit_hud::AggregateHullFraction(registry, vessel);
        if (entry.occupied) {
            entry.row.value = "LAUNCH";
        } else if (entry.owned) {
            entry.row.value = "BOARD";
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
        sr::ui::ListViewRowAt(layout.roster, static_cast<int>(roster.size()), 0.0f, input.cursor);
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

void Draw(const entt::registry& registry, const FactionId& playerFaction) {
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

    // Header: bay name (large, the screen's own identity) over one consolidated stat line --
    // slots, occupancy, and this bay hardpoint's integrity (features.md 3.4's mandatory per-screen
    // health readout) -- rather than a name/occupancy pair of lines plus a half-width gauge bar.
    // Closer to issue #224's reference, which reads the three facts as one line under the title.
    std::string bayName = "BAY";
    if (const DisplayName* stationName = registry.try_get<DisplayName>(station)) {
        bayName = stationName->value;
    }
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

    float integrityFraction = 1.0f;
    if (const Health* health = registry.try_get<Health>(thisBay);
        health != nullptr && health->max > 0.0f) {
        integrityFraction = health->current / health->max;
    }
    DrawHeader(layout.header, bayName, slots, occupied, integrityFraction);

    // Sibling selector: one TabStrip entry per living Docking hardpoint on the host, absent when
    // there is only one bay. Labelled phonetically (BAY ALPHA/BRAVO/...) to match the reference,
    // rather than the bare ordinal this screen drew before.
    if (showSiblingStrip) {
        DrawSiblingStrip(layout.siblingStrip, siblings, thisBay);
    }

    // Roster: view<Docked> filtered on docked.bay == thisBay, framed as its own bracket-bordered
    // sub-panel (DrawRosterPanel above). Your own vessel is a row like any other, marked via
    // `value` ("LAUNCH" if you occupy it, "BOARD" if you own it and don't) -- unchanged from
    // before, out of this issue's scope.
    const auto siblingIt = std::find(siblings.begin(), siblings.end(), thisBay);
    const std::size_t thisBayIndex =
        siblingIt != siblings.end() ? static_cast<std::size_t>(siblingIt - siblings.begin()) : 0;
    DrawRosterPanel(layout, thisBayIndex, slots);

    const std::vector<BayRosterEntry> roster =
        Roster(registry, thisBay, occupiedVessel, playerFaction);
    std::vector<sr::ui::Row> rows;
    rows.reserve(roster.size());
    for (const BayRosterEntry& entry : roster) {
        rows.push_back(entry.row);
    }
    sr::ui::DrawListView(layout.roster, rows, 0.0f, "BAY EMPTY");
}

}  // namespace sr::space::ui::bay_view
