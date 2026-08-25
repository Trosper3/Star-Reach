#include "modes/space/ui/StorageScreen.h"

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>

#include "modes/space/ui/BridgeView.h"
#include "shared/components/Docking.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::storage_screen {
namespace {

constexpr float kHeaderHeight = 54.0f;
constexpr float kSiblingStripHeight = 28.0f;
constexpr float kListLabelHeight = 22.0f;
constexpr float kListHeight = 220.0f;
constexpr float kColumnGap = 12.0f;
constexpr float kSectionGap = 10.0f;

// System.h's "one legitimate cache" exception (Law 6) -- duplicated locally per screen file on
// purpose, the same StorageMenu.cpp/EngineeringScreen.cpp precedent (architecture.md 12.30's
// established rule: each of this batch's files stays independently buildable).
entt::entity EnsureStateSingleton(entt::registry& registry) {
    for (auto [entity] : registry.view<StorageScreenStateSingleton>().each()) {
        return entity;
    }
    const entt::entity singleton = registry.create();
    registry.emplace<StorageScreenStateSingleton>(singleton);
    registry.emplace<StorageScreenState>(singleton);
    return singleton;
}

entt::entity FindStateSingleton(const entt::registry& registry) {
    for (auto [entity] : registry.view<StorageScreenStateSingleton>().each()) {
        return entity;
    }
    return entt::null;
}

StorageScreenState ReadState(const entt::registry& registry) {
    const entt::entity singleton = FindStateSingleton(registry);
    return singleton == entt::null ? StorageScreenState{}
                                   : registry.get<StorageScreenState>(singleton);
}

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

struct Layout {
    Rectangle header{};
    Rectangle leftStrip{};  // Zero height unless the vessel has more than one living hold.
    Rectangle leftPanel{};  // The bracket-bordered box wrapping leftLabel + left.
    Rectangle
        leftLabel{};   // "YOUR HOLD -- VANGUARD" / "Click a row to deposit ->", inside leftPanel.
    Rectangle left{};  // The ListView content rect -- what Update() hit-tests against.
    Rectangle rightStrip{};  // Zero height unless the station has more than one living hold.
    Rectangle rightPanel{};
    Rectangle rightLabel{};
    Rectangle right{};
};

// `showStripRow` reserves one shared strip row so both columns' lists stay vertically aligned --
// a side without its own sibling strip (one hold, or none) simply leaves that half blank rather
// than the two columns starting at different heights. `content` is
// bridge_view::FrameContentRect() -- already inset by the router's one bezel, so this lays
// sections out inside it directly rather than re-insetting via sr::ui::PanelContentRect, except
// for each panel's own interior, which gets exactly one nested inset (each hold is framed as its
// own sub-panel, per issue #225's reference).
Layout ComputeLayout(Rectangle content, bool showStripRow) {
    Layout layout;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    const float columnWidth = (content.width - kColumnGap) * 0.5f;
    float y = content.y + kHeaderHeight + kSectionGap;
    if (showStripRow) {
        layout.leftStrip = {content.x, y, columnWidth, kSiblingStripHeight};
        layout.rightStrip = {content.x + columnWidth + kColumnGap, y, columnWidth,
                             kSiblingStripHeight};
        y += kSiblingStripHeight + kSectionGap;
    }

    const float panelHeight = kListLabelHeight + kListHeight + sr::ui::kPanelPadding * 2.0f;
    layout.leftPanel = {content.x, y, columnWidth, panelHeight};
    layout.rightPanel = {content.x + columnWidth + kColumnGap, y, columnWidth, panelHeight};
    const Rectangle leftContent = sr::ui::PanelContentRect(layout.leftPanel);
    layout.leftLabel = {leftContent.x, leftContent.y, leftContent.width, kListLabelHeight};
    layout.left = {leftContent.x, leftContent.y + kListLabelHeight, leftContent.width, kListHeight};
    const Rectangle rightContent = sr::ui::PanelContentRect(layout.rightPanel);
    layout.rightLabel = {rightContent.x, rightContent.y, rightContent.width, kListLabelHeight};
    layout.right = {rightContent.x, rightContent.y + kListLabelHeight, rightContent.width,
                    kListHeight};
    return layout;
}

std::string FormatMass(float mass) {
    return std::to_string(static_cast<int>(mass));
}

// First two uppercase alphanumeric characters of a stack's id -- the same "no per-item art exists
// yet" placeholder shape BayView.cpp's BlueprintGlyph / RepairScreen.cpp's ShellGlyph use.
void ItemGlyph(const std::string& id, char (&out)[3]) {
    out[0] = '\0';
    out[1] = '\0';
    out[2] = '\0';
    int written = 0;
    for (const char c : id) {
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

// "YOUR HOLD" / "STATION HOLD" when that side has exactly one living hold, else the selected
// hold's own ordinal ("HOLD 2") -- mirrors HoldPillLabel's numbering without repeating the
// percentage the sibling strip above already shows.
std::string PanelTitle(const std::string& genericLabel, const std::vector<entt::entity>& holds,
                       entt::entity selected) {
    if (holds.size() <= 1) {
        return genericLabel;
    }
    const auto it = std::find(holds.begin(), holds.end(), selected);
    const std::size_t index = it != holds.end() ? static_cast<std::size_t>(it - holds.begin()) : 0;
    return "HOLD " + std::to_string(index + 1);
}

// "HOLD 2  74%" -- integrity folded into the label text (architecture.md 12.30.3's "each pill
// showing that hold's own integrity"), since TabStrip draws label strings only and has no
// per-tab colour channel of its own. Missing Health reads as full, the same default every other
// screen's own integrity readout uses.
std::string HoldPillLabel(const entt::registry& registry, entt::entity hold, std::size_t index) {
    std::string label = "HOLD " + std::to_string(index + 1);
    if (const Health* health = registry.try_get<Health>(hold);
        health != nullptr && health->max > 0.0f) {
        const int percent = static_cast<int>(100.0f * health->current / health->max);
        label += "  " + std::to_string(percent) + "%";
    }
    return label;
}

void DrawSiblingStrip(const entt::registry& registry, Rectangle bounds,
                      const std::vector<entt::entity>& holds, entt::entity selected) {
    if (holds.size() <= 1) {
        return;
    }
    std::vector<std::string> labels;
    labels.reserve(holds.size());
    int selectedIndex = -1;
    for (std::size_t i = 0; i < holds.size(); ++i) {
        labels.push_back(HoldPillLabel(registry, holds[i], i));
        if (holds[i] == selected) {
            selectedIndex = static_cast<int>(i);
        }
    }
    sr::ui::DrawTabStrip(bounds, labels, selectedIndex);
}

// architecture.md 2.2's function-length cap -- split out of Draw() below, one section each.

// Station name (large, the screen's own identity) over one consolidated stat line -- both holds'
// mass used/capacity -- rather than a name/occupancy pair of lines with no unit. Closer to issue
// #225's reference, which reads both figures as one line under the title.
void DrawHeader(Rectangle header, const std::string& stationName, float vesselUsed,
                float vesselCapacity, float stationUsed, float stationCapacity) {
    DrawText(stationName.c_str(), static_cast<int>(header.x), static_cast<int>(header.y), 24,
             sr::ui::kValueBright);

    int x = static_cast<int>(header.x);
    const int y = static_cast<int>(header.y + 30.0f);
    auto DrawStat = [&](const std::string& label, float used, float capacity) {
        DrawText(label.c_str(), x, y, 14, sr::ui::kLabelDim);
        x += MeasureText(label.c_str(), 14);
        const std::string value = FormatMass(used) + "/" + FormatMass(capacity) + " KG";
        DrawText(value.c_str(), x, y, 14, sr::ui::kValueBright);
        x += MeasureText(value.c_str(), 14);
    };
    DrawStat("YOUR HOLD ", vesselUsed, vesselCapacity);
    DrawText("    ", x, y, 14, sr::ui::kLabelDim);
    x += MeasureText("    ", 14);
    DrawStat("STATION HOLD ", stationUsed, stationCapacity);
}

// The reference's "YOUR HOLD -- VANGUARD" / "Click a row to deposit ->" box: a bracket panel plus
// a label row naming this side's selected hold and its owner, and a right-aligned hint for which
// way a click sends a stack. Drawn around (but not including) the ListView itself.
void DrawSidePanel(Rectangle panel, Rectangle label, const std::string& title,
                   const std::string& hint) {
    sr::ui::DrawBracketPanel(panel, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f, 2.0f);
    DrawText(title.c_str(), static_cast<int>(label.x), static_cast<int>(label.y), 14,
             sr::ui::kValueBright);
    const int hintWidth = MeasureText(hint.c_str(), 14);
    DrawText(hint.c_str(), static_cast<int>(label.x + label.width - hintWidth),
             static_cast<int>(label.y), 14, sr::ui::kLabelDim);
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
        ItemGlyph(stack.id, entry.row.glyph);
        entry.row.value = std::to_string(stack.quantity);
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

void Update(entt::registry& registry, const FactionId& playerFaction) {
    if (!bridge_view::IsStorageSelected(registry)) {
        return;
    }
    const entt::entity station = ActiveStation(registry, playerFaction);
    if (station == entt::null) {
        return;
    }
    const entt::entity vessel = OwnedVesselAt(registry, station, playerFaction);
    if (vessel == entt::null) {
        return;
    }

    const sr::ui::UiInput input{GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                GetMouseWheelMove()};
    if (!input.clicked) {
        return;
    }

    const std::vector<entt::entity> vesselHolds = SiblingHolds(registry, vessel);
    const std::vector<entt::entity> stationHolds = SiblingHolds(registry, station);
    const bool showStripRow = vesselHolds.size() > 1 || stationHolds.size() > 1;
    const Layout layout = ComputeLayout(bridge_view::FrameContentRect(), showStripRow);

    StorageScreenState& state = registry.get<StorageScreenState>(EnsureStateSingleton(registry));
    state.selectedVesselHold = ResolveSelectedHold(vesselHolds, state.selectedVesselHold);
    state.selectedStationHold = ResolveSelectedHold(stationHolds, state.selectedStationHold);

    if (vesselHolds.size() > 1) {
        if (const std::optional<int> hit = sr::ui::TabStripHitTest(
                layout.leftStrip, static_cast<int>(vesselHolds.size()), input.cursor);
            hit.has_value()) {
            state.selectedVesselHold = vesselHolds[static_cast<std::size_t>(*hit)];
            return;
        }
    }
    if (stationHolds.size() > 1) {
        if (const std::optional<int> hit = sr::ui::TabStripHitTest(
                layout.rightStrip, static_cast<int>(stationHolds.size()), input.cursor);
            hit.has_value()) {
            state.selectedStationHold = stationHolds[static_cast<std::size_t>(*hit)];
            return;
        }
    }

    const std::vector<StorageRow> yours = Rows(registry, vessel, station);
    if (const std::optional<int> hit =
            sr::ui::ListViewRowAt(layout.left, static_cast<int>(yours.size()), 0.0f, input.cursor);
        hit.has_value() && *hit < static_cast<int>(yours.size())) {
        const StorageRow& row = yours[static_cast<std::size_t>(*hit)];
        if (row.fits) {
            registry.emplace_or_replace<TransferItemRequest>(
                vessel, BuildDepositRequest(row, state.selectedStationHold));
        }
        return;
    }

    const std::vector<StorageRow> theirs = Rows(registry, station, vessel);
    if (const std::optional<int> hit = sr::ui::ListViewRowAt(
            layout.right, static_cast<int>(theirs.size()), 0.0f, input.cursor);
        hit.has_value() && *hit < static_cast<int>(theirs.size())) {
        const StorageRow& row = theirs[static_cast<std::size_t>(*hit)];
        if (row.fits) {
            registry.emplace_or_replace<TransferItemRequest>(
                vessel, BuildWithdrawRequest(row, state.selectedVesselHold));
        }
    }
}

void Draw(const entt::registry& registry, const FactionId& playerFaction) {
    if (!bridge_view::IsStorageSelected(registry)) {
        return;
    }
    const entt::entity station = ActiveStation(registry, playerFaction);
    if (station == entt::null) {
        return;
    }
    const entt::entity vessel = OwnedVesselAt(registry, station, playerFaction);
    if (vessel == entt::null) {
        return;  // No owned hull docked here yet to trade with.
    }

    const std::vector<entt::entity> vesselHolds = SiblingHolds(registry, vessel);
    const std::vector<entt::entity> stationHolds = SiblingHolds(registry, station);
    const bool showStripRow = vesselHolds.size() > 1 || stationHolds.size() > 1;
    const StorageScreenState state = ReadState(registry);
    const entt::entity selectedVesselHold =
        ResolveSelectedHold(vesselHolds, state.selectedVesselHold);
    const entt::entity selectedStationHold =
        ResolveSelectedHold(stationHolds, state.selectedStationHold);

    const Layout layout = ComputeLayout(bridge_view::FrameContentRect(), showStripRow);

    std::string stationName = "STORAGE";
    if (const DisplayName* name = registry.try_get<DisplayName>(station)) {
        stationName = name->value;
    }
    std::string vesselName = "VESSEL";
    if (const DisplayName* name = registry.try_get<DisplayName>(vessel)) {
        vesselName = name->value;
    }
    DrawHeader(layout.header, stationName, cargo_view::TotalMass(registry, vessel),
               cargo_view::Capacity(registry, vessel), cargo_view::TotalMass(registry, station),
               cargo_view::Capacity(registry, station));

    if (showStripRow) {
        DrawSiblingStrip(registry, layout.leftStrip, vesselHolds, selectedVesselHold);
        DrawSiblingStrip(registry, layout.rightStrip, stationHolds, selectedStationHold);
    }

    DrawSidePanel(layout.leftPanel, layout.leftLabel,
                  PanelTitle("YOUR HOLD", vesselHolds, selectedVesselHold) + " -- " + vesselName,
                  "Click a row to deposit ->");
    DrawSidePanel(
        layout.rightPanel, layout.rightLabel,
        PanelTitle("STATION HOLD", stationHolds, selectedStationHold) + " -- " + stationName,
        "<- Click a row to withdraw");

    const std::vector<StorageRow> yours = Rows(registry, vessel, station);
    std::vector<sr::ui::Row> yourRows;
    yourRows.reserve(yours.size());
    for (const StorageRow& entry : yours) {
        yourRows.push_back(entry.row);
    }
    sr::ui::DrawListView(layout.left, yourRows, 0.0f, "HOLD EMPTY");

    const std::vector<StorageRow> theirs = Rows(registry, station, vessel);
    std::vector<sr::ui::Row> theirRows;
    theirRows.reserve(theirs.size());
    for (const StorageRow& entry : theirs) {
        theirRows.push_back(entry.row);
    }
    sr::ui::DrawListView(layout.right, theirRows, 0.0f, "STATION HOLD EMPTY");
}

}  // namespace sr::space::ui::storage_screen
