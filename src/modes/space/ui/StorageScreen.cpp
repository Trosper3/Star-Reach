#include "modes/space/ui/StorageScreen.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include "modes/space/ui/BridgeView.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"
#include "shared/ui/Fonts.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::storage_screen {
namespace {

constexpr float kHeaderHeight = 54.0f;
constexpr float kSiblingStripHeight = 28.0f;
constexpr float kListLabelHeight = 22.0f;
constexpr float kStorageRowHeight = 48.0f;
constexpr float kStorageVisibleRows = 4.0f;
constexpr float kListHeight = kStorageRowHeight * kStorageVisibleRows;
constexpr float kCaptionHeight = 20.0f;
constexpr float kColumnGap = 12.0f;
constexpr float kSectionGap = 10.0f;
constexpr float kIconBoxSize = 30.0f;
constexpr float kPillWidth = 150.0f;
constexpr float kPillGap = 8.0f;

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

// The reference's "NO TRADE HARDPOINT -- DEPOSIT/WITHDRAW ONLY" badge is gated on this rather than
// shown unconditionally: architecture.md 12.30.3 ships Storage as the free deposit/withdraw half
// today regardless, but a station that already carries a living Trade hardpoint stops being
// accurately described as having "no trade hardpoint" the day Market ships (P6-08) and both
// screens can be true of the same station.
bool HasTradeHardpoint(const entt::registry& registry, entt::entity station) {
    const Rig* rig = registry.try_get<Rig>(station);
    if (rig == nullptr) {
        return false;
    }
    for (const entt::entity child : rig->children) {
        if (registry.all_of<Destroyed>(child)) {
            continue;
        }
        const FacilityRef* facility = registry.try_get<FacilityRef>(child);
        if (facility != nullptr && facility->kind == FacilityKind::Trade) {
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
    Rectangle left{};  // The row list content rect -- what Update() hit-tests against.
    Rectangle leftCaption{};  // The reference's italic footer line under the row list.
    Rectangle rightStrip{};   // Zero height unless the station has more than one living hold.
    Rectangle rightPanel{};
    Rectangle rightLabel{};
    Rectangle right{};
    Rectangle rightCaption{};
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

    const float panelHeight =
        kListLabelHeight + kListHeight + kCaptionHeight + sr::ui::kPanelPadding * 2.0f;
    layout.leftPanel = {content.x, y, columnWidth, panelHeight};
    layout.rightPanel = {content.x + columnWidth + kColumnGap, y, columnWidth, panelHeight};
    const Rectangle leftContent = sr::ui::PanelContentRect(layout.leftPanel);
    layout.leftLabel = {leftContent.x, leftContent.y, leftContent.width, kListLabelHeight};
    layout.left = {leftContent.x, leftContent.y + kListLabelHeight, leftContent.width, kListHeight};
    layout.leftCaption = {leftContent.x, layout.left.y + layout.left.height, leftContent.width,
                          kCaptionHeight};
    const Rectangle rightContent = sr::ui::PanelContentRect(layout.rightPanel);
    layout.rightLabel = {rightContent.x, rightContent.y, rightContent.width, kListLabelHeight};
    layout.right = {rightContent.x, rightContent.y + kListLabelHeight, rightContent.width,
                    kListHeight};
    layout.rightCaption = {rightContent.x, layout.right.y + layout.right.height, rightContent.width,
                           kCaptionHeight};
    return layout;
}

// "1240" -> "1,240" -- thousands-separated, matching the reference's mass readouts. Mass is never
// negative, so unlike BridgeView.cpp's FormatWithCommas (credits, which could in principle go
// negative) this skips a sign case.
std::string FormatMass(float mass) {
    std::string digits = std::to_string(static_cast<int>(mass));
    for (int pos = static_cast<int>(digits.size()) - 3; pos > 0; pos -= 3) {
        digits.insert(static_cast<std::size_t>(pos), ",");
    }
    return digits;
}

// features.md 3.9's status triad -- duplicated locally rather than shared, the established
// per-screen precedent (BayView.cpp/BridgeView.cpp's own IntegrityStatusColor).
Color IntegrityStatusColor(float fraction) {
    return fraction > 0.5f   ? sr::ui::kStatusGood
           : fraction > 0.2f ? sr::ui::kStatusCaution
                             : sr::ui::kStatusCritical;
}

// An outline pentagon for a raw ItemKind::Element (unrefined -- hollow) versus a filled diamond
// for an ItemKind::Module (manufactured -- solid): two placeholder shapes standing in for real
// per-item art (features.md 3.9), coarser than the reference's own per-category icon set (ore,
// plating, cell, weapon each drew differently) since nothing in `ItemStack` distinguishes a
// module's sub-category today -- that needs a content-registry lookup this screen does not have,
// not a fourth shape invented without data behind it.
void DrawItemGlyph(Rectangle box, ItemKind kind, Color color) {
    const Vector2 center{box.x + box.width / 2.0f, box.y + box.height / 2.0f};
    const float radius = box.width * 0.32f;
    if (kind == ItemKind::Element) {
        DrawPolyLines(center, 5, radius, -90.0f, color);
    } else {
        DrawPoly(center, 4, radius, 45.0f, BLACK);
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

// A hold's own integrity fraction -- 1.0 (full) when it carries no Health at all, the same
// "missing reads as full" default every other screen's readout uses.
float HoldIntegrityFraction(const entt::registry& registry, entt::entity hold) {
    const Health* health = registry.try_get<Health>(hold);
    return health != nullptr && health->max > 0.0f ? health->current / health->max : 1.0f;
}

// The reference's bordered pill: a status dot (features.md 3.9's colour-carries-condition triad)
// plus "HOLD 2" over its own integrity percentage -- two lines in one box, rather than the generic
// chamfered sr::ui::TabStrip (one line, no colour channel) Bay's own sibling-bay strip still uses.
// Storage's pills carry a fact (this hold's health) Bay's plain identity tabs do not, which is
// what earns the bespoke treatment here (Law 11: a widget shaped for the data it is showing).
void DrawHoldPill(Rectangle bounds, const sr::ui::Fonts& fonts, const std::string& title,
                  float integrityFraction, bool selected) {
    const Color chrome = selected ? sr::ui::kValueBright : sr::ui::kPanelChrome;
    DrawRectangleLinesEx(bounds, 1.0f, chrome);
    const Color dot = IntegrityStatusColor(integrityFraction);
    DrawCircle(static_cast<int>(bounds.x + 14.0f),
               static_cast<int>(bounds.y + bounds.height / 2.0f), 4.0f, dot);
    DrawTextEx(fonts.body, title.c_str(), {bounds.x + 26.0f, bounds.y + 7.0f}, 13.0f, 1.0f,
               sr::ui::kValueBright);
    const std::string percent =
        std::to_string(static_cast<int>(std::lround(integrityFraction * 100.0f))) + "%";
    DrawTextEx(fonts.body, percent.c_str(), {bounds.x + 26.0f, bounds.y + bounds.height - 18.0f},
               12.0f, 1.0f, sr::ui::kLabelDim);
}

// Pure -- fixed-width pills packed left to right, unlike sr::ui::TabStripHitTest's equal division
// of the whole strip: the reference's pills are compact boxes with a hint sentence filling the
// remainder, not tabs stretched to fill the row.
std::optional<int> HoldPillAt(Rectangle bounds, int count, Vector2 cursor) {
    for (int i = 0; i < count; ++i) {
        const Rectangle pill{bounds.x + static_cast<float>(i) * (kPillWidth + kPillGap), bounds.y,
                             kPillWidth, bounds.height};
        if (CheckCollisionPointRec(cursor, pill)) {
            return i;
        }
    }
    return std::nullopt;
}

void DrawSiblingStrip(const entt::registry& registry, Rectangle bounds, const sr::ui::Fonts& fonts,
                      const std::vector<entt::entity>& holds, entt::entity selected) {
    if (holds.size() <= 1) {
        return;
    }
    for (std::size_t i = 0; i < holds.size(); ++i) {
        const Rectangle pill{bounds.x + static_cast<float>(i) * (kPillWidth + kPillGap), bounds.y,
                             kPillWidth, bounds.height};
        DrawHoldPill(pill, fonts, "HOLD " + std::to_string(i + 1),
                     HoldIntegrityFraction(registry, holds[i]), holds[i] == selected);
    }
    const float hintX =
        bounds.x + static_cast<float>(holds.size()) * (kPillWidth + kPillGap) + 8.0f;
    DrawTextEx(fonts.body, "Deposits go to the hold selected above.",
               {hintX, bounds.y + bounds.height / 2.0f - 6.0f}, 11.0f, 1.0f, sr::ui::kLabelDim);
}

// architecture.md 2.2's function-length cap -- split out of Draw() below, one section each.

// "YOUR HOLD" / "STATION HOLD (ALL HOLDS)" -- the reference's "(BOTH BAYS)" generalised past
// exactly two, since SiblingHolds does not cap how many a rig may carry.
std::string StatLabel(const char* base, const std::vector<entt::entity>& holds) {
    return holds.size() > 1 ? std::string(base) + " (ALL HOLDS) " : std::string(base) + " ";
}

// A fixed "STORAGE" title (the router's top bar already names the specific station under
// "DOCKED AT" -- issue #225's Bay pass made the same call for "DOCKING BAY") plus, when this host
// has no Trade hardpoint of its own, the reference's caution-coloured "NO TRADE HARDPOINT --
// DEPOSIT/WITHDRAW ONLY" badge -- over one consolidated stat line for both holds' mass
// used/capacity.
void DrawHeader(Rectangle header, const sr::ui::Fonts& fonts, bool showTradeBadge,
                const std::string& yourLabel, const std::string& stationLabel, float vesselUsed,
                float vesselCapacity, float stationUsed, float stationCapacity) {
    DrawTextEx(fonts.heading, "STORAGE", {header.x, header.y}, 24.0f, 1.0f, sr::ui::kValueBright);
    if (showTradeBadge) {
        const Rectangle badge{header.x + 130.0f, header.y + 2.0f, 340.0f, 30.0f};
        DrawRectangleLinesEx(badge, 1.0f, sr::ui::kStatusCaution);
        DrawTextEx(fonts.body, "NO TRADE HARDPOINT -- DEPOSIT/WITHDRAW",
                   {badge.x + 8.0f, badge.y + 4.0f}, 11.0f, 1.0f, sr::ui::kStatusCaution);
        DrawTextEx(fonts.body, "ONLY", {badge.x + 8.0f, badge.y + 16.0f}, 11.0f, 1.0f,
                   sr::ui::kStatusCaution);
    }

    float x = header.x;
    const float y = header.y + 30.0f;
    auto DrawStat = [&](const std::string& label, float used, float capacity) {
        DrawTextEx(fonts.body, label.c_str(), {x, y}, 14.0f, 1.0f, sr::ui::kLabelDim);
        x += MeasureTextEx(fonts.body, label.c_str(), 14.0f, 1.0f).x;
        const std::string value = FormatMass(used) + " / " + FormatMass(capacity) + " KG";
        DrawTextEx(fonts.body, value.c_str(), {x, y}, 14.0f, 1.0f, sr::ui::kValueBright);
        x += MeasureTextEx(fonts.body, value.c_str(), 14.0f, 1.0f).x;
    };
    DrawStat(yourLabel, vesselUsed, vesselCapacity);
    DrawTextEx(fonts.body, " -- ", {x, y}, 14.0f, 1.0f, sr::ui::kLabelDim);
    x += MeasureTextEx(fonts.body, " -- ", 14.0f, 1.0f).x;
    DrawStat(stationLabel, stationUsed, stationCapacity);
}

// The reference's "YOUR HOLD -- VANGUARD" / "Click a row to deposit ->" box: a bracket panel, a
// label row naming this side's selected hold and its owner with a right-aligned directional hint,
// a divider rule under it (the mock's own hairline every panel header has, added in the same pass
// as Bay's roster panel), and the reference's italic footer caption under the row list.
void DrawSidePanel(Rectangle panel, Rectangle label, Rectangle caption, const sr::ui::Fonts& fonts,
                   const std::string& title, const std::string& hint,
                   const std::string& captionText) {
    sr::ui::DrawBracketPanel(panel, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f, 2.0f);
    DrawTextEx(fonts.body, title.c_str(), {label.x, label.y}, 14.0f, 1.0f, sr::ui::kValueBright);
    const float hintWidth = MeasureTextEx(fonts.body, hint.c_str(), 14.0f, 1.0f).x;
    DrawTextEx(fonts.body, hint.c_str(), {label.x + label.width - hintWidth, label.y}, 14.0f, 1.0f,
               sr::ui::kLabelDim);
    const float dividerY = label.y + label.height;
    DrawLineEx({label.x, dividerY}, {label.x + label.width, dividerY}, 1.0f, sr::ui::kDivider);
    DrawTextEx(fonts.body, captionText.c_str(), {caption.x, caption.y}, 11.0f, 1.0f,
               sr::ui::kLabelDim);
}

// One storage row: an icon box (border carries condition -- dim once disabled, chrome otherwise;
// DrawItemGlyph's shape carries identity), the item's id, and its quantity -- red once the stack
// won't fit ("x6 FULL"), dim otherwise. A single line, unlike Bay's two-line roster card: a
// commodity stack has no subtitle or verb of its own the way a vessel row's Board/Launch does.
void DrawStorageRow(Rectangle bounds, const sr::ui::Fonts& fonts, const StorageRow& entry) {
    const Rectangle iconBox{bounds.x, bounds.y + (bounds.height - kIconBoxSize) / 2.0f,
                            kIconBoxSize, kIconBoxSize};
    const Color chrome = entry.row.style.disabled ? sr::ui::kLabelDim : sr::ui::kPanelChrome;
    DrawRectangleLinesEx(iconBox, 1.0f, chrome);
    DrawItemGlyph(iconBox, entry.kind, chrome);

    const float textX = iconBox.x + iconBox.width + 14.0f;
    const Color labelColor = entry.row.style.disabled ? sr::ui::kLabelDim : sr::ui::kValueBright;
    DrawTextEx(fonts.heading, entry.row.label.c_str(),
               {textX, bounds.y + bounds.height / 2.0f - 8.0f}, 15.0f, 1.0f, labelColor);

    const Color valueColor = entry.row.style.disabled ? sr::ui::kStatusCritical : sr::ui::kLabelDim;
    const float valueWidth = MeasureTextEx(fonts.body, entry.row.value.c_str(), 14.0f, 1.0f).x;
    DrawTextEx(fonts.body, entry.row.value.c_str(),
               {bounds.x + bounds.width - valueWidth, bounds.y + bounds.height / 2.0f - 7.0f},
               14.0f, 1.0f, valueColor);
}

// The row list, top to bottom inside `bounds`, divider rules between rows -- the generic
// sr::ui::ListView draws one flat text line with an inline monogram glyph and has no bordered icon
// box of its own, so this screen draws its own rather than stretch that widget to a shape only
// two screens need so far (Law 11 -- Bay's own roster is the other, and the two rows do not agree
// on line count, so neither is "the" shape to generalise from yet).
void DrawStorageRows(Rectangle bounds, const sr::ui::Fonts& fonts,
                     const std::vector<StorageRow>& rows, const std::string& emptyMessage) {
    BeginScissorMode(static_cast<int>(bounds.x), static_cast<int>(bounds.y),
                     static_cast<int>(bounds.width), static_cast<int>(bounds.height));
    if (rows.empty()) {
        DrawTextEx(fonts.body, emptyMessage.c_str(), {bounds.x, bounds.y}, 14.0f, 1.0f,
                   sr::ui::kLabelDim);
        EndScissorMode();
        return;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const float y = bounds.y + static_cast<float>(i) * kStorageRowHeight;
        if (y > bounds.y + bounds.height) {
            break;
        }
        if (i > 0) {
            DrawLineEx({bounds.x, y}, {bounds.x + bounds.width, y}, 1.0f, sr::ui::kDivider);
        }
        DrawStorageRow({bounds.x, y, bounds.width, kStorageRowHeight}, fonts, rows[i]);
    }
    EndScissorMode();
}

// Pure -- the row-card analog of sr::ui::ListViewRowAt, using kStorageRowHeight instead of
// sr::ui::kListRowHeight. No scroll offset, matching Bay's own RosterRowAt: the panel is sized for
// kStorageVisibleRows and scrolling past that is out of this pass's scope.
std::optional<int> StorageRowAt(Rectangle bounds, int rowCount, Vector2 cursor) {
    if (rowCount <= 0 || !CheckCollisionPointRec(cursor, bounds)) {
        return std::nullopt;
    }
    const int index = static_cast<int>((cursor.y - bounds.y) / kStorageRowHeight);
    if (index < 0 || index >= rowCount) {
        return std::nullopt;
    }
    return index;
}

}  // namespace

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
        if (const std::optional<int> hit =
                HoldPillAt(layout.leftStrip, static_cast<int>(vesselHolds.size()), input.cursor);
            hit.has_value()) {
            state.selectedVesselHold = vesselHolds[static_cast<std::size_t>(*hit)];
            return;
        }
    }
    if (stationHolds.size() > 1) {
        if (const std::optional<int> hit =
                HoldPillAt(layout.rightStrip, static_cast<int>(stationHolds.size()), input.cursor);
            hit.has_value()) {
            state.selectedStationHold = stationHolds[static_cast<std::size_t>(*hit)];
            return;
        }
    }

    const std::vector<StorageRow> yours = Rows(registry, vessel, station);
    if (const std::optional<int> hit =
            StorageRowAt(layout.left, static_cast<int>(yours.size()), input.cursor);
        hit.has_value() && *hit < static_cast<int>(yours.size())) {
        const StorageRow& row = yours[static_cast<std::size_t>(*hit)];
        if (row.fits) {
            registry.emplace_or_replace<TransferItemRequest>(
                vessel, BuildDepositRequest(row, state.selectedStationHold));
        }
        return;
    }

    const std::vector<StorageRow> theirs = Rows(registry, station, vessel);
    if (const std::optional<int> hit =
            StorageRowAt(layout.right, static_cast<int>(theirs.size()), input.cursor);
        hit.has_value() && *hit < static_cast<int>(theirs.size())) {
        const StorageRow& row = theirs[static_cast<std::size_t>(*hit)];
        if (row.fits) {
            registry.emplace_or_replace<TransferItemRequest>(
                vessel, BuildWithdrawRequest(row, state.selectedVesselHold));
        }
    }
}

void Draw(const entt::registry& registry, const FactionId& playerFaction,
          const sr::ui::Fonts& fonts) {
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
    DrawHeader(layout.header, fonts, !HasTradeHardpoint(registry, station),
               StatLabel("YOUR HOLD", vesselHolds), StatLabel("STATION HOLD", stationHolds),
               cargo_view::TotalMass(registry, vessel), cargo_view::Capacity(registry, vessel),
               cargo_view::TotalMass(registry, station), cargo_view::Capacity(registry, station));

    if (showStripRow) {
        DrawSiblingStrip(registry, layout.leftStrip, fonts, vesselHolds, selectedVesselHold);
        DrawSiblingStrip(registry, layout.rightStrip, fonts, stationHolds, selectedStationHold);
    }

    DrawSidePanel(layout.leftPanel, layout.leftLabel, layout.leftCaption, fonts,
                  PanelTitle("YOUR HOLD", vesselHolds, selectedVesselHold) + " -- " + vesselName,
                  "Click a row to deposit ->", "Deposit is free -- no ownership boundary crossed.");
    DrawSidePanel(
        layout.rightPanel, layout.rightLabel, layout.rightCaption, fonts,
        PanelTitle("STATION HOLD", stationHolds, selectedStationHold) + " -- " + stationName,
        "<- Click a row to withdraw",
        "A disabled row means the stack won't fit in your hold right now.");

    const std::vector<StorageRow> yours = Rows(registry, vessel, station);
    DrawStorageRows(layout.left, fonts, yours, "HOLD EMPTY");

    const std::vector<StorageRow> theirs = Rows(registry, station, vessel);
    DrawStorageRows(layout.right, fonts, theirs, "STATION HOLD EMPTY");
}

// Diverges from the reference mock on purpose: the mock's top-bar gauge reads the vessel's own
// cargo fill, but every other screen's gauge (Bay's "BAY ALPHA -- 100%" foremost) reads a facility
// hardpoint's own structural health -- features.md 3.4's "you die with your facility" readout.
// Storage's facility is the STATION's cargo hold (the thing that can be shot and destroyed while
// you are stood here trading with it), not your own hull, which CockpitHud already covers
// elsewhere -- so this reads the selected station hold's own integrity, matching Bay's pattern
// rather than the mock's fill-level one.
std::optional<bridge_view::GaugeStatus> ActiveGaugeStatus(const entt::registry& registry,
                                                          const FactionId& playerFaction) {
    if (!bridge_view::IsStorageSelected(registry)) {
        return std::nullopt;
    }
    const entt::entity station = ActiveStation(registry, playerFaction);
    if (station == entt::null) {
        return std::nullopt;
    }
    if (OwnedVesselAt(registry, station, playerFaction) == entt::null) {
        return std::nullopt;
    }
    const std::vector<entt::entity> stationHolds = SiblingHolds(registry, station);
    const StorageScreenState state = ReadState(registry);
    const entt::entity selectedStationHold =
        ResolveSelectedHold(stationHolds, state.selectedStationHold);
    const std::string label = PanelTitle("STATION HOLD", stationHolds, selectedStationHold);
    if (selectedStationHold == entt::null) {
        return bridge_view::GaugeStatus{label, 1.0f};
    }
    return bridge_view::GaugeStatus{label, HoldIntegrityFraction(registry, selectedStationHold)};
}

}  // namespace sr::space::ui::storage_screen
