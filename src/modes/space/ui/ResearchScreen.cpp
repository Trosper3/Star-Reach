#include "modes/space/ui/ResearchScreen.h"

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <unordered_set>

#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/NetworkOwner.h"
#include "shared/components/Research.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"
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
    const int whole = static_cast<int>(seconds + 0.5f);
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

constexpr float kPanelWidth = 560.0f;
constexpr float kPanelTop = 132.0f;
constexpr float kHeaderHeight = 44.0f;
constexpr float kSectionLabelHeight = 20.0f;
constexpr float kListHeight = 160.0f;

struct Layout {
    Rectangle header{};
    Rectangle candidateLabel{};
    Rectangle candidateList{};
    Rectangle queueLabel{};
    Rectangle queueList{};
};

Rectangle PanelBounds() {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float height =
        kHeaderHeight + 2.0f * (kSectionLabelHeight + kListHeight) + 2.0f * sr::ui::kPanelPadding;
    return Rectangle{(screenWidth - kPanelWidth) * 0.5f, kPanelTop, kPanelWidth, height};
}

Layout ComputeLayout() {
    const Rectangle content = sr::ui::PanelContentRect(PanelBounds());
    Layout layout;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    float y = content.y + kHeaderHeight;
    layout.candidateLabel = {content.x, y, content.width, kSectionLabelHeight};
    y += kSectionLabelHeight;
    layout.candidateList = {content.x, y, content.width, kListHeight};
    y += kListHeight;
    layout.queueLabel = {content.x, y, content.width, kSectionLabelHeight};
    y += kSectionLabelHeight;
    layout.queueList = {content.x, y, content.width, kListHeight};
    return layout;
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
        entry.row.glyph[0] = static_cast<char>(std::toupper(stack.id.empty() ? '?' : stack.id[0]));
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

    const NetworkOwner* owner = registry.try_get<NetworkOwner>(requester);
    const core::knowledge::KnowledgeNetwork* network =
        owner != nullptr ? knowledge.Get(owner->network) : nullptr;
    const StationFacility* stationFacility = registry.try_get<StationFacility>(station);
    const int queuedCount =
        stationFacility != nullptr ? static_cast<int>(stationFacility->researchJobs.size()) : 0;

    const std::vector<CandidateRow> candidates =
        Candidates(registry, requester, facility, station, network, queuedCount,
                   FacilityCapacity(registry, facility));

    const Layout layout = ComputeLayout();
    const std::optional<int> hit = sr::ui::ListViewRowAt(
        layout.candidateList, static_cast<int>(candidates.size()), 0.0f, input.cursor);
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

void Draw(const entt::registry& registry, const FactionId& playerFaction,
          const core::knowledge::KnowledgeStore& knowledge) {
    const entt::entity shell = PlayerShell(registry);
    const entt::entity facility = CurrentFacility(registry, shell);
    if (facility == entt::null) {
        return;
    }
    const entt::entity station = registry.get<ParentRig>(facility).root;
    const entt::entity requester = OwnedVesselAt(registry, station, playerFaction);

    const Layout layout = ComputeLayout();
    sr::ui::DrawPanelFrame(PanelBounds());

    std::string labName = "RESEARCH LAB";
    if (const DisplayName* name = registry.try_get<DisplayName>(station)) {
        labName = name->value;
    }
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

    DrawText((labName + "  GRADE " + std::to_string(grade)).c_str(),
             static_cast<int>(layout.header.x), static_cast<int>(layout.header.y), 18,
             sr::ui::kValueBright);
    DrawText(("SLOTS " + slots).c_str(), static_cast<int>(layout.header.x),
             static_cast<int>(layout.header.y + 20.0f), 14, sr::ui::kLabelDim);

    const Health* facilityHealth = registry.try_get<Health>(facility);
    if (facilityHealth != nullptr) {
        const float integrity =
            facilityHealth->max > 0.0f ? facilityHealth->current / facilityHealth->max : 1.0f;
        const Color gaugeColor = integrity > 0.5f   ? sr::ui::kStatusGood
                                 : integrity > 0.2f ? sr::ui::kStatusCaution
                                                    : sr::ui::kStatusCritical;
        const Rectangle gaugeBounds{layout.header.x + layout.header.width * 0.5f, layout.header.y,
                                    layout.header.width * 0.5f, 20.0f};
        sr::ui::DrawGauge(gaugeBounds, "FACILITY INTEGRITY", integrity, gaugeColor);
    }

    DrawText("CARGO -- CANDIDATE SAMPLES", static_cast<int>(layout.candidateLabel.x),
             static_cast<int>(layout.candidateLabel.y), 14, sr::ui::kLabelDim);
    const NetworkOwner* owner =
        requester != entt::null ? registry.try_get<NetworkOwner>(requester) : nullptr;
    const core::knowledge::KnowledgeNetwork* network =
        owner != nullptr ? knowledge.Get(owner->network) : nullptr;
    const std::vector<CandidateRow> candidates =
        Candidates(registry, requester, facility, station, network, queuedCount, capacity);
    std::vector<sr::ui::Row> candidateWidgetRows;
    candidateWidgetRows.reserve(candidates.size());
    for (const CandidateRow& entry : candidates) {
        candidateWidgetRows.push_back(entry.row);
    }
    sr::ui::DrawListView(layout.candidateList, candidateWidgetRows, 0.0f, "NOTHING TO RESEARCH");

    DrawText("QUEUE", static_cast<int>(layout.queueLabel.x), static_cast<int>(layout.queueLabel.y),
             14, sr::ui::kLabelDim);
    const std::vector<sr::ui::Row> queueRows = QueueRows(registry, station, facility);
    sr::ui::DrawListView(layout.queueList, queueRows, 0.0f, "NO ACTIVE RESEARCH");
}

}  // namespace sr::space::ui::research_screen
