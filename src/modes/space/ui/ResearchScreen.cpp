#include "modes/space/ui/ResearchScreen.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <unordered_set>

#include "modes/space/ui/BridgeView.h"
#include "modes/space/ui/CodexScreen.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/NetworkOwner.h"
#include "shared/components/Research.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"
#include "shared/ui/Fonts.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::research_screen {
namespace {

// Duplicated from modes/space/systems/ResearchSystem.cpp's own private table -- modes/*/ui/ may
// not include a systems/ file (architecture.md section 2.3), so the pre-commit preview this
// screen owes the player (features.md 2.4: duration shown before the click) needs its own copy
// of the same features.md 2.4 table. ResearchSystem.h's public DurationSeconds documents this
// same duplication from the system side; keep the two numerically identical if either changes.
constexpr float kBaseDurationSeconds = 60.0f;
constexpr float kGradeTimeFactor[7] = {1.00f, 0.88f, 0.76f, 0.64f, 0.52f, 0.40f, 0.30f};

float DurationSeconds(int facilityGrade) {
    const int index = std::clamp(facilityGrade, 1, 7) - 1;
    return kBaseDurationSeconds * kGradeTimeFactor[index];
}

std::string FormatSeconds(float seconds) {
    const int whole = static_cast<int>(std::lround(seconds));
    if (whole < 60) {
        return std::to_string(whole) + "s";
    }
    return std::to_string(whole / 60) + "m " + std::to_string(whole % 60) + "s";
}

entt::entity PlayerShell(const entt::registry& registry) {
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        return entity;
    }
    return entt::null;
}

// The living Research-kind facility hardpoint PlayerLocation currently names, or entt::null --
// the same "this screen is active exactly while standing on its own gate hardpoint" pattern
// modes/space/ui/BayView.h's CurrentBay and RepairScreen.h's CurrentFacility establish.
entt::entity CurrentFacility(const entt::registry& registry, entt::entity shell) {
    if (shell == entt::null || registry.all_of<Destroyed>(shell)) {
        return entt::null;
    }
    const FacilityRef* facility = registry.try_get<FacilityRef>(shell);
    if (facility == nullptr || facility->kind != FacilityKind::Research) {
        return entt::null;
    }
    // architecture.md 12.30's frame: at most one full-screen tab at a time, and Storage's own
    // selection (bridge_view::IsStorageSelected) has no hardpoint to contest this gate with
    // otherwise.
    if (bridge_view::IsStorageSelected(registry)) {
        return entt::null;
    }
    return shell;
}

MountId FacilityMount(const entt::registry& registry, entt::entity facility) {
    const MountRef* mount = registry.try_get<MountRef>(facility);
    return mount != nullptr ? mount->id : MountId();
}

int FacilityGrade(const entt::registry& registry, entt::entity facility) {
    const FacilityRef* ref = registry.try_get<FacilityRef>(facility);
    return ref != nullptr ? ref->grade : 1;
}

int FacilityCapacity(const entt::registry& registry, entt::entity facility) {
    const FacilityRef* ref = registry.try_get<FacilityRef>(facility);
    return ref != nullptr ? ref->capacity : 0;
}

constexpr float kHeaderHeight = 54.0f;
constexpr float kSectionGap = 10.0f;
constexpr float kListLabelHeight = 22.0f;
constexpr float kRowHeight = 48.0f;
constexpr float kVisibleRows = 4.0f;
constexpr float kListHeight = kRowHeight * kVisibleRows;
constexpr float kIconBoxSize = 30.0f;
constexpr float kCodexButtonWidth = 118.0f;
constexpr float kCodexButtonHeight = 30.0f;
constexpr float kRowButtonWidth = 86.0f;
constexpr float kRowButtonHeight = 26.0f;

// The bracket-bordered box wrapping one section (candidates or queue): a label row naming the
// section with a right-aligned hint, a divider rule under it, and the row-list content rect --
// mirrors Repair's/Storage's own SectionLayout.
struct SectionLayout {
    Rectangle panel{};
    Rectangle label{};
    Rectangle list{};
};

struct Layout {
    Rectangle header{};
    Rectangle codexButton{};
    SectionLayout candidates{};
    SectionLayout queue{};
};

// `content` is bridge_view::FrameContentRect() -- already inset by the router's one bezel, so
// this lays sections out inside it directly rather than re-insetting via sr::ui::PanelContentRect,
// except for each panel's own interior, which gets exactly one nested inset (each section is
// framed as its own sub-panel, per issue #227's reference -- Repair's/Storage's own panel
// treatment).
Layout ComputeLayout(Rectangle content) {
    Layout layout;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    layout.codexButton = {content.x + content.width - kCodexButtonWidth, content.y,
                          kCodexButtonWidth, kCodexButtonHeight};

    const float panelHeight = kListLabelHeight + kListHeight + sr::ui::kPanelPadding * 2.0f;
    float y = content.y + kHeaderHeight + kSectionGap;

    layout.candidates.panel = {content.x, y, content.width, panelHeight};
    const Rectangle candidatesInner = sr::ui::PanelContentRect(layout.candidates.panel);
    layout.candidates.label = {candidatesInner.x, candidatesInner.y, candidatesInner.width,
                               kListLabelHeight};
    layout.candidates.list = {candidatesInner.x, candidatesInner.y + kListLabelHeight,
                              candidatesInner.width, kListHeight};
    y += panelHeight + kSectionGap;

    layout.queue.panel = {content.x, y, content.width, panelHeight};
    const Rectangle queueInner = sr::ui::PanelContentRect(layout.queue.panel);
    layout.queue.label = {queueInner.x, queueInner.y, queueInner.width, kListLabelHeight};
    layout.queue.list = {queueInner.x, queueInner.y + kListLabelHeight, queueInner.width,
                         kListHeight};
    return layout;
}

// Pure -- the bordered-icon-box row analog of sr::ui::ListViewRowAt, using kRowHeight instead of
// sr::ui::kListRowHeight. No scroll offset, matching Bay's/Storage's/Repair's own row-at
// functions: the panel is sized for kVisibleRows and scrolling past that is out of this pass's
// scope.
std::optional<int> ResearchRowAt(Rectangle bounds, int rowCount, Vector2 cursor) {
    if (rowCount <= 0 || !CheckCollisionPointRec(cursor, bounds)) {
        return std::nullopt;
    }
    const int index = static_cast<int>((cursor.y - bounds.y) / kRowHeight);
    if (index < 0 || index >= rowCount) {
        return std::nullopt;
    }
    return index;
}

}  // namespace

entt::entity OwnedVesselAt(const entt::registry& registry, entt::entity station,
                           const FactionId& playerFaction) {
    for (auto [vessel, docked, faction] : registry.view<Docked, FactionRef>().each()) {
        if (docked.station == station && faction.id == playerFaction) {
            return vessel;
        }
    }
    return entt::null;
}

std::vector<CandidateRow> Candidates(const entt::registry& registry, entt::entity requester,
                                     entt::entity facility, entt::entity station,
                                     const core::knowledge::KnowledgeNetwork* network,
                                     int queuedCount, int capacity) {
    std::vector<CandidateRow> rows;
    if (requester == entt::null) {
        return rows;
    }

    const StationFacility* stationFacility = registry.try_get<StationFacility>(station);
    const MountId mount = FacilityMount(registry, facility);
    const int grade = FacilityGrade(registry, facility);
    const bool full = capacity != 0 && queuedCount >= capacity;

    std::unordered_set<std::string> seen;
    for (const ItemStack& stack : cargo_view::Merged(registry, requester)) {
        if (stack.kind != ItemKind::Module || !seen.insert(stack.id).second) {
            continue;
        }

        CandidateRow entry;
        entry.item = ModuleId(stack.id);
        entry.alreadyKnown = network != nullptr && network->unlockedBlueprints.count(stack.id) != 0;
        entry.noSlots = full;
        entry.alreadyQueued =
            stationFacility != nullptr &&
            std::any_of(stationFacility->researchJobs.begin(), stationFacility->researchJobs.end(),
                        [&](const ResearchJob& job) {
                            return job.facility == mount && job.item == entry.item;
                        });

        entry.row.label = stack.id;
        entry.row.style.disabled = entry.alreadyKnown || entry.noSlots || entry.alreadyQueued;
        entry.row.value = entry.alreadyKnown    ? "ALREADY KNOWN"
                          : entry.alreadyQueued ? "QUEUED"
                          : entry.noSlots       ? "NO SLOTS"
                                                : FormatSeconds(DurationSeconds(grade));
        rows.push_back(std::move(entry));
    }
    return rows;
}

std::vector<sr::ui::Row> QueueRows(const entt::registry& registry, entt::entity station,
                                   entt::entity facility) {
    std::vector<sr::ui::Row> rows;
    const StationFacility* stationFacility = registry.try_get<StationFacility>(station);
    if (stationFacility == nullptr) {
        return rows;
    }
    const MountId mount = FacilityMount(registry, facility);

    for (const ResearchJob& job : stationFacility->researchJobs) {
        if (job.facility != mount) {
            continue;
        }
        sr::ui::Row row;
        row.label = job.item.str();
        const float remaining = std::max(0.0f, job.durationSeconds - job.progress);
        row.value = FormatSeconds(remaining) + " left";
        row.fill = job.durationSeconds > 0.0f ? job.progress / job.durationSeconds : 0.0f;
        rows.push_back(std::move(row));
    }
    return rows;
}

std::optional<bridge_view::GaugeStatus> ActiveGaugeStatus(const entt::registry& registry,
                                                          const FactionId& playerFaction) {
    const entt::entity shell = PlayerShell(registry);
    const entt::entity facility = CurrentFacility(registry, shell);
    if (facility == entt::null) {
        return std::nullopt;
    }
    const entt::entity station = registry.get<ParentRig>(facility).root;
    if (OwnedVesselAt(registry, station, playerFaction) == entt::null) {
        return std::nullopt;
    }
    const Health* health = registry.try_get<Health>(facility);
    const float fraction =
        health != nullptr && health->max > 0.0f ? health->current / health->max : 1.0f;
    return bridge_view::GaugeStatus{"RESEARCH LAB", fraction};
}

void Update(entt::registry& registry, const FactionId& playerFaction,
            const core::knowledge::KnowledgeStore& knowledge) {
    const entt::entity shell = PlayerShell(registry);
    const entt::entity facility = CurrentFacility(registry, shell);
    if (facility == entt::null) {
        return;
    }
    const entt::entity station = registry.get<ParentRig>(facility).root;
    const entt::entity requester = OwnedVesselAt(registry, station, playerFaction);
    if (requester == entt::null) {
        return;
    }

    const sr::ui::UiInput input{GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                GetMouseWheelMove()};
    if (!input.clicked) {
        return;
    }

    const Layout layout = ComputeLayout(bridge_view::FrameContentRect());
    if (sr::ui::ButtonClicked(layout.codexButton, input)) {
        codex_screen::Open(registry);
        return;
    }

    const NetworkOwner* owner = registry.try_get<NetworkOwner>(requester);
    const core::knowledge::KnowledgeNetwork* network =
        owner != nullptr ? knowledge.Get(owner->network) : nullptr;
    const StationFacility* stationFacility = registry.try_get<StationFacility>(station);
    const int queuedCount =
        stationFacility != nullptr ? static_cast<int>(stationFacility->researchJobs.size()) : 0;

    const std::vector<CandidateRow> candidates =
        Candidates(registry, requester, facility, station, network, queuedCount,
                   FacilityCapacity(registry, facility));

    const std::optional<int> hit =
        ResearchRowAt(layout.candidates.list, static_cast<int>(candidates.size()), input.cursor);
    if (!hit.has_value() || *hit >= static_cast<int>(candidates.size())) {
        return;
    }

    const CandidateRow& row = candidates[static_cast<std::size_t>(*hit)];
    if (row.row.style.disabled) {
        return;
    }
    registry.emplace_or_replace<StartResearchRequest>(
        requester, StartResearchRequest{row.item, FacilityMount(registry, facility)});
}

namespace {

// architecture.md 2.2's function-length cap -- split out of Draw() below, one section each.

// A fixed "RESEARCH LAB" title (the router's top bar already names the specific station under
// "DOCKED AT" -- Bay's/Storage's/Repair's own precedent) over one consolidated GRADE/SLOTS stat
// line, plus the CODEX button top-right. No integrity readout here any more -- ActiveGaugeStatus
// feeds that to the router's top-bar Gauge instead, the same call Repair's/Storage's own headers
// already made.
void DrawHeader(Rectangle header, Rectangle codexButton, const sr::ui::Fonts& fonts, int grade,
                const std::string& slots) {
    DrawTextEx(fonts.heading, "RESEARCH LAB", {header.x, header.y}, 24.0f, 1.0f,
               sr::ui::kValueBright);

    float x = header.x;
    const float y = header.y + 30.0f;
    auto DrawStat = [&](const std::string& label, const std::string& value) {
        DrawTextEx(fonts.body, label.c_str(), {x, y}, 14.0f, 1.0f, sr::ui::kLabelDim);
        x += MeasureTextEx(fonts.body, label.c_str(), 14.0f, 1.0f).x;
        DrawTextEx(fonts.body, value.c_str(), {x, y}, 14.0f, 1.0f, sr::ui::kValueBright);
        x += MeasureTextEx(fonts.body, value.c_str(), 14.0f, 1.0f).x;
    };
    DrawStat("GRADE ", std::to_string(grade));
    DrawTextEx(fonts.body, " -- ", {x, y}, 14.0f, 1.0f, sr::ui::kLabelDim);
    x += MeasureTextEx(fonts.body, " -- ", 14.0f, 1.0f).x;
    DrawStat("SLOTS ", slots);

    sr::ui::DrawChamferedButton(codexButton, "VIEW CODEX", fonts.body, 13.0f, sr::ui::kPanelGlass,
                                sr::ui::kPanelChrome, sr::ui::kValueBright);
}

// The label row + divider shared by both sections: a title naming the section and a right-
// aligned hint, over a hairline rule -- mirrors Repair's/Storage's own SectionLayout label
// treatment.
void DrawSectionPanel(const SectionLayout& section, const sr::ui::Fonts& fonts,
                      const std::string& title, const std::string& hint) {
    sr::ui::DrawBracketPanel(section.panel, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f, 2.0f);
    DrawTextEx(fonts.body, title.c_str(), {section.label.x, section.label.y}, 14.0f, 1.0f,
               sr::ui::kValueBright);
    const float hintWidth = MeasureTextEx(fonts.body, hint.c_str(), 14.0f, 1.0f).x;
    DrawTextEx(fonts.body, hint.c_str(),
               {section.label.x + section.label.width - hintWidth, section.label.y}, 14.0f, 1.0f,
               sr::ui::kLabelDim);
    const float dividerY = section.label.y + section.label.height;
    DrawLineEx({section.label.x, dividerY}, {section.label.x + section.label.width, dividerY}, 1.0f,
               sr::ui::kDivider);
}

// One candidate row: a bordered icon box (a filled diamond -- every candidate here is
// ItemKind::Module, the same shape Storage's own DrawItemGlyph draws for a Module stack), the
// item's raw id, and either a "RESEARCH" button (enabled rows -- still hit-tested as the whole
// row by Update(), the click-a-row model issue #227 leaves unchanged) or the row's already-
// formatted refusal text, right-aligned and dim (disabled rows).
void DrawCandidateRow(Rectangle bounds, const sr::ui::Fonts& fonts, const CandidateRow& entry) {
    const Rectangle iconBox{bounds.x, bounds.y + (bounds.height - kIconBoxSize) / 2.0f,
                            kIconBoxSize, kIconBoxSize};
    const Color chrome = entry.row.style.disabled ? sr::ui::kLabelDim : sr::ui::kPanelChrome;
    DrawRectangleLinesEx(iconBox, 1.0f, chrome);
    const Vector2 center{iconBox.x + iconBox.width / 2.0f, iconBox.y + iconBox.height / 2.0f};
    DrawPoly(center, 4, iconBox.width * 0.32f, 45.0f, BLACK);

    const float textX = iconBox.x + iconBox.width + 14.0f;
    const Color labelColor = entry.row.style.disabled ? sr::ui::kLabelDim : sr::ui::kValueBright;
    DrawTextEx(fonts.heading, entry.row.label.c_str(),
               {textX, bounds.y + bounds.height / 2.0f - 8.0f}, 15.0f, 1.0f, labelColor);

    if (entry.row.style.disabled) {
        const float valueWidth = MeasureTextEx(fonts.body, entry.row.value.c_str(), 13.0f, 1.0f).x;
        DrawTextEx(fonts.body, entry.row.value.c_str(),
                   {bounds.x + bounds.width - valueWidth, bounds.y + bounds.height / 2.0f - 6.0f},
                   13.0f, 1.0f, sr::ui::kLabelDim);
        return;
    }
    const Rectangle button{bounds.x + bounds.width - kRowButtonWidth,
                           bounds.y + (bounds.height - kRowButtonHeight) / 2.0f, kRowButtonWidth,
                           kRowButtonHeight};
    sr::ui::DrawChamferedButton(button, "RESEARCH", fonts.body, 12.0f, sr::ui::kPanelGlass,
                                sr::ui::kPanelChrome, sr::ui::kValueBright);
}

// The candidate list, top to bottom inside `bounds`, divider rules between rows -- Bay's/
// Storage's/Repair's own bordered-icon-box row treatment, replacing the generic
// sr::ui::DrawListView this screen drew before (issue #227's visual-chrome pass).
void DrawCandidateRows(Rectangle bounds, const sr::ui::Fonts& fonts,
                       const std::vector<CandidateRow>& rows) {
    BeginScissorMode(static_cast<int>(bounds.x), static_cast<int>(bounds.y),
                     static_cast<int>(bounds.width), static_cast<int>(bounds.height));
    if (rows.empty()) {
        DrawTextEx(fonts.body, "NOTHING TO RESEARCH", {bounds.x, bounds.y}, 14.0f, 1.0f,
                   sr::ui::kLabelDim);
        EndScissorMode();
        return;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const float y = bounds.y + static_cast<float>(i) * kRowHeight;
        if (y > bounds.y + bounds.height) {
            break;
        }
        if (i > 0) {
            DrawLineEx({bounds.x, y}, {bounds.x + bounds.width, y}, 1.0f, sr::ui::kDivider);
        }
        DrawCandidateRow({bounds.x, y, bounds.width, kRowHeight}, fonts, rows[i]);
    }
    EndScissorMode();
}

// One running-job row: the same bordered icon box as a candidate row, `row.fill`'s progress
// painted behind it across the whole row (sr::ui::GaugeFillRect, the pure half DrawGauge already
// shares), the item's raw id, and the already-formatted "{time} left" text right-aligned.
void DrawQueueRow(Rectangle bounds, const sr::ui::Fonts& fonts, const sr::ui::Row& row) {
    if (row.fill >= 0.0f) {
        DrawRectangleRec(sr::ui::GaugeFillRect(bounds, row.fill), sr::ui::kPanelChrome);
    }

    const Rectangle iconBox{bounds.x, bounds.y + (bounds.height - kIconBoxSize) / 2.0f,
                            kIconBoxSize, kIconBoxSize};
    DrawRectangleLinesEx(iconBox, 1.0f, sr::ui::kPanelChrome);
    const Vector2 center{iconBox.x + iconBox.width / 2.0f, iconBox.y + iconBox.height / 2.0f};
    DrawPoly(center, 4, iconBox.width * 0.32f, 45.0f, BLACK);

    const float textX = iconBox.x + iconBox.width + 14.0f;
    DrawTextEx(fonts.heading, row.label.c_str(), {textX, bounds.y + bounds.height / 2.0f - 8.0f},
               15.0f, 1.0f, sr::ui::kValueBright);

    const float valueWidth = MeasureTextEx(fonts.body, row.value.c_str(), 13.0f, 1.0f).x;
    DrawTextEx(fonts.body, row.value.c_str(),
               {bounds.x + bounds.width - valueWidth, bounds.y + bounds.height / 2.0f - 6.0f},
               13.0f, 1.0f, sr::ui::kLabelDim);
}

// The queue list, top to bottom inside `bounds`, divider rules between rows -- mirrors
// DrawCandidateRows above.
void DrawQueueRows(Rectangle bounds, const sr::ui::Fonts& fonts,
                   const std::vector<sr::ui::Row>& rows) {
    BeginScissorMode(static_cast<int>(bounds.x), static_cast<int>(bounds.y),
                     static_cast<int>(bounds.width), static_cast<int>(bounds.height));
    if (rows.empty()) {
        DrawTextEx(fonts.body, "NO ACTIVE RESEARCH", {bounds.x, bounds.y}, 14.0f, 1.0f,
                   sr::ui::kLabelDim);
        EndScissorMode();
        return;
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const float y = bounds.y + static_cast<float>(i) * kRowHeight;
        if (y > bounds.y + bounds.height) {
            break;
        }
        if (i > 0) {
            DrawLineEx({bounds.x, y}, {bounds.x + bounds.width, y}, 1.0f, sr::ui::kDivider);
        }
        DrawQueueRow({bounds.x, y, bounds.width, kRowHeight}, fonts, rows[i]);
    }
    EndScissorMode();
}

}  // namespace

void Draw(const entt::registry& registry, const FactionId& playerFaction,
          const core::knowledge::KnowledgeStore& knowledge, const sr::ui::Fonts& fonts) {
    const entt::entity shell = PlayerShell(registry);
    const entt::entity facility = CurrentFacility(registry, shell);
    if (facility == entt::null) {
        return;
    }
    const entt::entity station = registry.get<ParentRig>(facility).root;
    const entt::entity requester = OwnedVesselAt(registry, station, playerFaction);
    if (requester == entt::null) {
        return;
    }

    const Layout layout = ComputeLayout(bridge_view::FrameContentRect());

    const int grade = FacilityGrade(registry, facility);
    const int capacity = FacilityCapacity(registry, facility);
    const StationFacility* stationFacility = registry.try_get<StationFacility>(station);
    const MountId mount = FacilityMount(registry, facility);
    int queuedCount = 0;
    if (stationFacility != nullptr) {
        queuedCount = static_cast<int>(std::count_if(
            stationFacility->researchJobs.begin(), stationFacility->researchJobs.end(),
            [&](const ResearchJob& job) { return job.facility == mount; }));
    }
    const std::string slots =
        capacity == 0 ? (std::to_string(queuedCount) + " / UNLIMITED")
                      : (std::to_string(queuedCount) + " / " + std::to_string(capacity));
    DrawHeader(layout.header, layout.codexButton, fonts, grade, slots);

    const NetworkOwner* owner = registry.try_get<NetworkOwner>(requester);
    const core::knowledge::KnowledgeNetwork* network =
        owner != nullptr ? knowledge.Get(owner->network) : nullptr;
    const std::vector<CandidateRow> candidates =
        Candidates(registry, requester, facility, station, network, queuedCount, capacity);
    DrawSectionPanel(layout.candidates, fonts, "CANDIDATES -- FROM YOUR CARGO",
                     "Costs a slot & time, not the item");
    DrawCandidateRows(layout.candidates.list, fonts, candidates);

    const std::vector<sr::ui::Row> queueRows = QueueRows(registry, station, facility);
    DrawSectionPanel(layout.queue, fonts, "ACTIVE QUEUE -- THIS LAB",
                     std::to_string(queueRows.size()) + " RUNNING");
    DrawQueueRows(layout.queue.list, fonts, queueRows);
}

}  // namespace sr::space::ui::research_screen
