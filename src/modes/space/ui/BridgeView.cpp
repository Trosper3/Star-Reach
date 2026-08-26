#include "modes/space/ui/BridgeView.h"

#include <raylib.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/ui/Fonts.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::bridge_view {
namespace {

constexpr std::size_t kFacilityKindCount = static_cast<std::size_t>(FacilityKind::Engineering) + 1;

// architecture.md 12.30's "a tab needs a working screen behind it, not just a living hardpoint"
// fix: compile-time, flips one entry the day its screen's Draw is wired into SpaceFlight.cpp.
// Market ships with P6-08; Manufacturing's Queue half ships between P4-07 (Draft only, already
// landed) and P6-03/P6-06.
bool IsScreenShipped(ScreenId screen) {
    switch (screen) {
        case ScreenId::Bay:
        case ScreenId::Storage:
        case ScreenId::Repair:
        case ScreenId::Engineering:
        case ScreenId::Research: return true;
        case ScreenId::Market:
        case ScreenId::Manufacturing: return false;
    }
    return false;
}

ScreenId ScreenIdFor(FacilityKind kind) {
    switch (kind) {
        case FacilityKind::Docking: return ScreenId::Bay;
        case FacilityKind::Trade: return ScreenId::Market;
        case FacilityKind::Repair: return ScreenId::Repair;
        case FacilityKind::Engineering: return ScreenId::Engineering;
        case FacilityKind::Manufacturing: return ScreenId::Manufacturing;
        case FacilityKind::Research: return ScreenId::Research;
    }
    return ScreenId::Bay;
}

// issue #225's visual-chrome pass: a top bar (station identity, ownership, credits, the active
// screen's Gauge) over a left-hand vertical icon rail, replacing the old single horizontal tab
// strip -- matching the Docking Screens Redesign reference rather than the placeholder text-tab
// row that shipped with architecture.md 12.30's frame.
constexpr float kTopBarHeight = 60.0f;
constexpr float kSidebarWidth = 84.0f;
constexpr float kSidebarCellHeight = 84.0f;

// architecture.md 12.30's frame: the whole window, bezel included -- not a small centered panel.
Rectangle WindowBounds() {
    return Rectangle{0.0f, 0.0f, static_cast<float>(GetScreenWidth()),
                     static_cast<float>(GetScreenHeight())};
}

Rectangle TopBarBounds(Rectangle content) {
    return Rectangle{content.x, content.y, content.width, kTopBarHeight};
}

// The row below the top bar, shared by the sidebar and the content area -- `content` is already
// PanelContentRect(WindowBounds()), so this is the one place the vertical gap between them lives.
Rectangle BelowTopBarBounds(Rectangle content) {
    const float y = content.y + kTopBarHeight + sr::ui::kPanelPadding;
    return Rectangle{content.x, y, content.width, std::max(0.0f, content.y + content.height - y)};
}

Rectangle SidebarBounds(Rectangle content) {
    const Rectangle below = BelowTopBarBounds(content);
    return Rectangle{below.x, below.y, kSidebarWidth, below.height};
}

// Pure -- the vertical analog of sr::ui::TabStripHitTest, one fixed-height cell per tab stacked
// from the top of `bounds` rather than `bounds.height / tabCount` equal division: the mock's rail
// keeps every icon cell the same size regardless of how many tabs a given station has, rather
// than stretching three cells to fill a five-cell column. Sidebar rendering has exactly one
// consumer (this file), so this stays local rather than joining shared/ui/Widgets.h (Law 11).
std::optional<int> SidebarHitTest(Rectangle bounds, int tabCount, Vector2 cursor) {
    if (tabCount <= 0 || !CheckCollisionPointRec(cursor, bounds)) {
        return std::nullopt;
    }
    const int index = static_cast<int>((cursor.y - bounds.y) / kSidebarCellHeight);
    if (index < 0 || index >= tabCount) {
        return std::nullopt;
    }
    return index;
}

// System.h's "one legitimate cache" exception (Law 6), the SystemMenuState/CommsLog precedent --
// created lazily the first time the Storage tab is clicked, not up front. Kept file-local: nothing
// outside this file writes it, and every reader goes through IsStorageSelected below.
struct StorageSelectedSingleton {};

entt::entity FindStorageSelected(const entt::registry& registry) {
    for (auto [entity] : registry.view<StorageSelectedSingleton>().each()) {
        return entity;
    }
    return entt::null;
}

void SetStorageSelected(entt::registry& registry) {
    if (FindStorageSelected(registry) == entt::null) {
        registry.emplace<StorageSelectedSingleton>(registry.create());
    }
}

void ClearStorageSelected(entt::registry& registry) {
    if (const entt::entity entity = FindStorageSelected(registry); entity != entt::null) {
        registry.destroy(entity);
    }
}

// The entity currently carrying PlayerLocation (architecture.md 12.30.1 -- exactly one per
// registry), or entt::null if OnEnter hasn't placed a player.
entt::entity PlayerShell(const entt::registry& registry) {
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        return entity;
    }
    return entt::null;
}

// "4820" -> "4,820" -- thousands-separated, the mock's credits readout. Negative amounts (a
// faction cannot go below zero today, but this stays honest if that ever changes) get their sign
// carried past the grouping rather than grouped into it.
std::string FormatWithCommas(int amount) {
    std::string digits = std::to_string(std::abs(amount));
    for (int pos = static_cast<int>(digits.size()) - 3; pos > 0; pos -= 3) {
        digits.insert(static_cast<std::size_t>(pos), ",");
    }
    return amount < 0 ? "-" + digits : digits;
}

// features.md 3.9's three-stop status triad (the same buckets BayView's own IntegrityStatusColor
// uses for its header stat line) -- duplicated locally rather than shared, the established
// per-screen precedent (BlueprintGlyph/ShellGlyph) for a three-line helper neither file may
// depend on the other for.
Color IntegrityStatusColor(float fraction) {
    return fraction > 0.5f   ? sr::ui::kStatusGood
           : fraction > 0.2f ? sr::ui::kStatusCaution
                             : sr::ui::kStatusCritical;
}

// Whether `station` is the player's own (green "OWNED" badge) or someone else's (a neutral
// "STATION" badge) -- `label` is FactionDisplayName either way, or "UNCLAIMED" for a station with
// no FactionRef at all (a wreck or an unclaimed hulk, not gated out of the docked frame itself).
struct OwnershipBadge {
    bool owned = false;
    std::string label;
};

OwnershipBadge ResolveOwnershipBadge(const entt::registry& registry, entt::entity station,
                                     const FactionId& playerFaction) {
    const FactionRef* faction = registry.try_get<FactionRef>(station);
    if (faction == nullptr) {
        return OwnershipBadge{false, "UNCLAIMED"};
    }
    return OwnershipBadge{faction->id == playerFaction, FactionDisplayName(faction->id)};
}

// True if any of `station`'s living hardpoints has taken damage -- the Repair tab's notification
// dot (the mock's own example). Deliberately health-based rather than FacilityKind::Repair-
// specific: any damaged hardpoint is a reason to visit Repair, not just a damaged repair bay.
bool StationNeedsRepair(const entt::registry& registry, entt::entity station) {
    const Rig* rig = registry.try_get<Rig>(station);
    if (rig == nullptr) {
        return false;
    }
    for (const entt::entity child : rig->children) {
        if (registry.all_of<Destroyed>(child)) {
            continue;
        }
        if (const Health* health = registry.try_get<Health>(child);
            health != nullptr && health->current < health->max) {
            return true;
        }
    }
    return false;
}

// architecture.md 2.2's function-length cap -- split out of Draw() below, one section each.

void DrawTopBar(Rectangle bounds, const sr::ui::Fonts& fonts, const std::string& stationName,
                const OwnershipBadge& badge, int credits,
                const std::optional<GaugeStatus>& activeGauge) {
    DrawTextEx(fonts.body, "DOCKED AT", {bounds.x, bounds.y}, 12.0f, 1.0f, sr::ui::kLabelDim);
    DrawTextEx(fonts.heading, stationName.c_str(), {bounds.x, bounds.y + 16.0f}, 22.0f, 1.0f,
               sr::ui::kValueBright);

    const Rectangle badgeBounds{bounds.x + 280.0f, bounds.y + 2.0f, 230.0f, 36.0f};
    const Color badgeColor = badge.owned ? sr::ui::kStatusGood : sr::ui::kPanelChrome;
    DrawRectangleLinesEx(badgeBounds, 1.5f, badgeColor);
    DrawCircle(static_cast<int>(badgeBounds.x + 14.0f), static_cast<int>(badgeBounds.y + 18.0f),
               3.5f, badgeColor);
    const std::string badgeText = (badge.owned ? "OWNED -- " : "STATION -- ") + badge.label;
    DrawTextEx(fonts.body, badgeText.c_str(), {badgeBounds.x + 24.0f, badgeBounds.y + 11.0f}, 12.0f,
               1.0f, badgeColor);

    float rightEdge = bounds.x + bounds.width;
    if (activeGauge.has_value()) {
        constexpr float kGaugeWidth = 170.0f;
        const Rectangle gaugeBounds{rightEdge - kGaugeWidth, bounds.y + 10.0f, kGaugeWidth, 18.0f};
        sr::ui::DrawGauge(gaugeBounds, activeGauge->label, activeGauge->fraction,
                          IntegrityStatusColor(activeGauge->fraction), fonts.body);
        rightEdge = gaugeBounds.x - 24.0f;
    }
    const std::string creditsText = FormatWithCommas(credits) + " CR";
    const float creditsWidth = MeasureTextEx(fonts.body, creditsText.c_str(), 14.0f, 1.0f).x;
    DrawTextEx(fonts.body, creditsText.c_str(), {rightEdge - creditsWidth, bounds.y + 14.0f}, 14.0f,
               1.0f, sr::ui::kValueBright);
}

// One placeholder vector glyph per ScreenId, centred in `box` -- the "no per-item art exists yet"
// convention (BlueprintGlyph et al.) applied to icons instead of monogram letters, since the
// sidebar's icons carry no per-station identity to distinguish, only which screen a cell opens.
void DrawScreenIcon(ScreenId screen, Rectangle box, Color color) {
    const float cx = box.x + box.width / 2.0f;
    const float cy = box.y + box.height / 2.0f;
    switch (screen) {
        case ScreenId::Bay: {
            DrawRectangleLinesEx({cx - 12.0f, cy - 10.0f, 24.0f, 20.0f}, 1.5f, color);
            DrawTriangle({cx - 4.0f, cy - 5.0f}, {cx - 4.0f, cy + 5.0f}, {cx + 6.0f, cy}, color);
            break;
        }
        case ScreenId::Storage: {
            for (int i = 0; i < 6; ++i) {
                const float a0 = static_cast<float>(i) * (PI / 3.0f);
                const float a1 = static_cast<float>(i + 1) * (PI / 3.0f);
                DrawLineEx({cx + std::cos(a0) * 13.0f, cy + std::sin(a0) * 13.0f},
                           {cx + std::cos(a1) * 13.0f, cy + std::sin(a1) * 13.0f}, 1.5f, color);
            }
            break;
        }
        case ScreenId::Repair: {
            DrawCircleLines(static_cast<int>(cx - 8.0f), static_cast<int>(cy - 8.0f), 4.0f, color);
            DrawCircleLines(static_cast<int>(cx + 8.0f), static_cast<int>(cy + 8.0f), 4.0f, color);
            DrawLineEx({cx - 5.0f, cy - 5.0f}, {cx + 5.0f, cy + 5.0f}, 2.0f, color);
            break;
        }
        case ScreenId::Engineering: {
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 5.0f, color);
            for (int i = 0; i < 8; ++i) {
                const float angle = static_cast<float>(i) * (PI / 4.0f);
                DrawLineEx({cx + std::cos(angle) * 8.0f, cy + std::sin(angle) * 8.0f},
                           {cx + std::cos(angle) * 13.0f, cy + std::sin(angle) * 13.0f}, 1.5f,
                           color);
            }
            break;
        }
        case ScreenId::Research: {
            DrawLineEx({cx - 4.0f, cy - 12.0f}, {cx - 4.0f, cy}, 1.5f, color);
            DrawLineEx({cx + 4.0f, cy - 12.0f}, {cx + 4.0f, cy}, 1.5f, color);
            DrawLineEx({cx - 4.0f, cy - 12.0f}, {cx + 4.0f, cy - 12.0f}, 1.5f, color);
            DrawLineEx({cx - 4.0f, cy}, {cx - 10.0f, cy + 12.0f}, 1.5f, color);
            DrawLineEx({cx + 4.0f, cy}, {cx + 10.0f, cy + 12.0f}, 1.5f, color);
            DrawLineEx({cx - 10.0f, cy + 12.0f}, {cx + 10.0f, cy + 12.0f}, 1.5f, color);
            break;
        }
        case ScreenId::Market:
        case ScreenId::Manufacturing: {
            DrawRectangleLinesEx({cx - 10.0f, cy - 10.0f, 20.0f, 20.0f}, 1.5f, color);
            break;
        }
    }
}

void DrawSidebar(Rectangle bounds, const sr::ui::Fonts& fonts, const std::vector<BridgeTab>& tabs,
                 int selected, bool repairNeedsAttention) {
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        const Rectangle cell{bounds.x, bounds.y + static_cast<float>(i) * kSidebarCellHeight,
                             bounds.width, kSidebarCellHeight};
        const bool isSelected = static_cast<int>(i) == selected;
        if (isSelected) {
            DrawRectangleRec(cell, Color{sr::ui::kPanelChrome.r, sr::ui::kPanelChrome.g,
                                         sr::ui::kPanelChrome.b, 35});
            DrawRectangleRec({cell.x, cell.y, 3.0f, cell.height}, sr::ui::kPanelChrome);
        }
        const Color tint = isSelected ? sr::ui::kValueBright : sr::ui::kLabelDim;
        DrawScreenIcon(tabs[i].screen, {cell.x, cell.y + 8.0f, cell.width, cell.height * 0.5f},
                       tint);

        const std::string label(ToString(tabs[i].screen));
        const float labelWidth = MeasureTextEx(fonts.body, label.c_str(), 12.0f, 1.0f).x;
        DrawTextEx(fonts.body, label.c_str(),
                   {cell.x + (cell.width - labelWidth) / 2.0f, cell.y + cell.height * 0.68f}, 12.0f,
                   1.0f, tint);

        if (tabs[i].screen == ScreenId::Repair && repairNeedsAttention) {
            DrawCircle(static_cast<int>(cell.x + cell.width - 16.0f),
                       static_cast<int>(cell.y + 16.0f), 4.0f, sr::ui::kStatusCaution);
        }
    }
}

}  // namespace

entt::entity DockedStation(const entt::registry& registry) {
    const entt::entity shell = PlayerShell(registry);
    if (shell == entt::null) {
        return entt::null;
    }

    // Standing inside a facility hardpoint (a screen): its ParentRig::root IS the station --
    // exactly what the derived PlayerControlled already resolves to (architecture.md 12.30.1),
    // read directly here instead so this keeps working even though PlayerControlled itself never
    // carries Docked in this state.
    if (registry.all_of<FacilityRef>(shell)) {
        const ParentRig* parent = registry.try_get<ParentRig>(shell);
        return parent != nullptr ? parent->root : entt::null;
    }

    // Aboard a vessel -- the player's own cockpit (self-referential, no ParentRig) or a boarded
    // hull (architecture.md 12.30.2's Board). Docked lives on the rig root either way.
    entt::entity root = shell;
    if (const ParentRig* parent = registry.try_get<ParentRig>(shell)) {
        root = parent->root;
    }
    const Docked* docked = registry.try_get<Docked>(root);
    return docked != nullptr ? docked->station : entt::null;
}

std::string FactionDisplayName(const FactionId& faction) {
    std::string text = faction.str();
    for (char& c : text) {
        c = c == '_' ? ' ' : static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return text;
}

std::string_view ToString(ScreenId value) {
    switch (value) {
        case ScreenId::Bay: return "BAY";
        case ScreenId::Market: return "MARKET";
        case ScreenId::Storage: return "STORAGE";
        case ScreenId::Repair: return "REPAIR";
        case ScreenId::Engineering: return "ENGINEERING";
        case ScreenId::Manufacturing: return "MANUFACTURING";
        case ScreenId::Research: return "RESEARCH";
    }
    return "BAY";
}

std::vector<BridgeTab> AvailableTabs(const entt::registry& registry, entt::entity rigRoot) {
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return {};
    }

    std::array<entt::entity, kFacilityKindCount> firstByKind{};
    firstByKind.fill(entt::null);
    bool hasCargoHold = false;

    for (const entt::entity child : rig->children) {
        if (registry.all_of<Destroyed>(child)) {
            continue;
        }
        if (const FacilityRef* facility = registry.try_get<FacilityRef>(child)) {
            entt::entity& slot = firstByKind[static_cast<std::size_t>(facility->kind)];
            if (slot == entt::null) {
                slot = child;
            }
        }
        if (registry.all_of<CargoHold>(child)) {
            hasCargoHold = true;
        }
    }

    // ScreenId declaration order, not discovery order (matches the old kAllKinds contract) --
    // the tab list must not reshuffle as hardpoints are destroyed and rebuilt across a session.
    // Each candidate is gated on IsScreenShipped in addition to the hardpoint (or CargoHold)
    // living -- architecture.md 12.30's "a tab needs a working screen behind it" fix. A station's
    // capabilities and the game's readiness are two different questions; only the first belongs
    // to the per-hardpoint checks above.
    std::vector<BridgeTab> tabs;
    const entt::entity docking = firstByKind[static_cast<std::size_t>(FacilityKind::Docking)];
    if (docking != entt::null && IsScreenShipped(ScreenId::Bay)) {
        tabs.push_back({ScreenId::Bay, docking});
    }
    const entt::entity trade = firstByKind[static_cast<std::size_t>(FacilityKind::Trade)];
    if (trade != entt::null && IsScreenShipped(ScreenId::Market)) {
        tabs.push_back({ScreenId::Market, trade});
    }
    if (hasCargoHold && IsScreenShipped(ScreenId::Storage)) {
        // No FacilityKind, no hardpoint identity -- architecture.md 12.30.3: stands alone on a
        // station with a CargoHold and no Trade hardpoint, rides the Market tab otherwise.
        tabs.push_back({ScreenId::Storage, entt::null});
    }
    const entt::entity repair = firstByKind[static_cast<std::size_t>(FacilityKind::Repair)];
    if (repair != entt::null && IsScreenShipped(ScreenId::Repair)) {
        tabs.push_back({ScreenId::Repair, repair});
    }
    const entt::entity engineering =
        firstByKind[static_cast<std::size_t>(FacilityKind::Engineering)];
    if (engineering != entt::null && IsScreenShipped(ScreenId::Engineering)) {
        tabs.push_back({ScreenId::Engineering, engineering});
    }
    const entt::entity manufacturing =
        firstByKind[static_cast<std::size_t>(FacilityKind::Manufacturing)];
    if (manufacturing != entt::null && IsScreenShipped(ScreenId::Manufacturing)) {
        tabs.push_back({ScreenId::Manufacturing, manufacturing});
    }
    const entt::entity research = firstByKind[static_cast<std::size_t>(FacilityKind::Research)];
    if (research != entt::null && IsScreenShipped(ScreenId::Research)) {
        tabs.push_back({ScreenId::Research, research});
    }
    return tabs;
}

void SelectTab(entt::registry& registry, entt::entity shell, std::span<const BridgeTab> tabs,
               int tabIndex) {
    if (tabIndex < 0 || static_cast<std::size_t>(tabIndex) >= tabs.size()) {
        return;
    }
    const BridgeTab& tab = tabs[static_cast<std::size_t>(tabIndex)];
    if (tab.screen == ScreenId::Storage) {
        // Nothing physical to move PlayerLocation onto (architecture.md 12.30.3) -- the selection
        // itself is what makes Storage the shown screen.
        SetStorageSelected(registry);
        return;
    }
    ClearStorageSelected(registry);
    if (tab.hardpoint == entt::null || tab.hardpoint == shell) {
        return;
    }
    registry.remove<PlayerLocation>(shell);
    registry.emplace<PlayerLocation>(tab.hardpoint, PlayerLocation{tab.hardpoint});
}

void Update(entt::registry& registry) {
    const entt::entity station = DockedStation(registry);
    if (station == entt::null) {
        ClearStorageSelected(registry);
        return;
    }

    const std::vector<BridgeTab> tabs = AvailableTabs(registry, station);
    const sr::ui::UiInput input{GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                GetMouseWheelMove()};
    if (!input.clicked) {
        return;
    }

    const Rectangle content = sr::ui::PanelContentRect(WindowBounds());
    const std::optional<int> hit =
        SidebarHitTest(SidebarBounds(content), static_cast<int>(tabs.size()), input.cursor);
    if (!hit.has_value()) {
        return;
    }

    const entt::entity shell = PlayerShell(registry);
    if (shell == entt::null) {
        return;
    }
    SelectTab(registry, shell, tabs, *hit);
}

void Draw(const entt::registry& registry, const FactionId& playerFaction,
          const core::economy::FactionEconomy& economy, const sr::ui::Fonts& fonts,
          const std::optional<GaugeStatus>& activeGauge) {
    const entt::entity station = DockedStation(registry);
    if (station == entt::null) {
        return;
    }

    const std::vector<BridgeTab> tabs = AvailableTabs(registry, station);
    // The one full-window bezel for whichever screen is about to draw inside it -- see this
    // function's own header comment.
    const Rectangle content = sr::ui::DrawPanelFrame(WindowBounds());

    std::string stationName = "STATION";
    if (const DisplayName* name = registry.try_get<DisplayName>(station)) {
        stationName = name->value;
    }
    DrawTopBar(TopBarBounds(content), fonts, stationName,
               ResolveOwnershipBadge(registry, station, playerFaction),
               economy.Stock(playerFaction), activeGauge);

    const entt::entity shell = PlayerShell(registry);
    const bool storageSelected = IsStorageSelected(registry);
    int selected = -1;
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        const bool isThisTab =
            storageSelected ? tabs[i].screen == ScreenId::Storage
                            : (tabs[i].hardpoint != entt::null && tabs[i].hardpoint == shell);
        if (isThisTab) {
            selected = static_cast<int>(i);
            break;
        }
    }

    DrawSidebar(SidebarBounds(content), fonts, tabs, selected,
                StationNeedsRepair(registry, station));

    // The reference's two hairlines this frame was missing: one under the top bar (full width),
    // one along the icon rail's right edge (full height below it) -- both drawn once here rather
    // than by every screen, the same "drawn once by the router" argument this file's own header
    // comment already makes for the bezel and edge channel.
    const float topBarBottom = content.y + kTopBarHeight;
    DrawLineEx({content.x, topBarBottom}, {content.x + content.width, topBarBottom}, 1.0f,
               sr::ui::kDivider);
    const Rectangle sidebar = SidebarBounds(content);
    DrawLineEx({sidebar.x + sidebar.width, sidebar.y},
               {sidebar.x + sidebar.width, sidebar.y + sidebar.height}, 1.0f, sr::ui::kDivider);
}

Rectangle FrameContentRect() {
    const Rectangle content = sr::ui::PanelContentRect(WindowBounds());
    const Rectangle below = BelowTopBarBounds(content);
    const float left = below.x + kSidebarWidth + sr::ui::kPanelPadding;
    return Rectangle{left, below.y, std::max(0.0f, below.x + below.width - left), below.height};
}

bool IsStorageSelected(const entt::registry& registry) {
    return FindStorageSelected(registry) != entt::null;
}

}  // namespace sr::space::ui::bridge_view
