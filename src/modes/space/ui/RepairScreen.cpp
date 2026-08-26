#include "modes/space/ui/RepairScreen.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

#include "core/economy/Pricing.h"
#include "modes/space/ui/BridgeView.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/components/StationServices.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::repair_screen {
namespace {

constexpr float kHeaderHeight = 54.0f;
constexpr float kSectionGap = 10.0f;
constexpr float kColumnGap = 12.0f;
constexpr float kLabelHeight = 22.0f;
constexpr float kListHeight = 200.0f;
constexpr float kFooterGap = 6.0f;
constexpr float kFooterHeight = 26.0f;
constexpr float kHintGap = 4.0f;
constexpr float kHintHeight = 16.0f;
constexpr float kFooterButtonWidth = 100.0f;

// One-letter monogram per ShellKind (features.md 3.9's glyph-carries-identity rule) -- the same
// placeholder RefactorMenu.cpp's own ShellGlyph uses; duplicated locally rather than shared,
// since neither file may depend on the other and this is the whole of it.
void ShellGlyph(ShellKind kind, char (&out)[3]) {
    switch (kind) {
        case ShellKind::Chassis: out[0] = 'C'; break;
        case ShellKind::Armor: out[0] = 'A'; break;
        case ShellKind::PowerCell: out[0] = 'P'; break;
        case ShellKind::Engine: out[0] = 'E'; break;
        case ShellKind::Weapon: out[0] = 'W'; break;
        case ShellKind::Shield: out[0] = 'S'; break;
        case ShellKind::Facility: out[0] = 'F'; break;
    }
    out[1] = '\0';
    out[2] = '\0';
}

entt::entity PlayerShell(const entt::registry& registry) {
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        return entity;
    }
    return entt::null;
}

// The living Repair-kind facility hardpoint PlayerLocation currently names, or entt::null. The
// same "this screen is active exactly while standing on its own gate hardpoint" pattern
// modes/space/ui/BayView.h's CurrentBay establishes.
entt::entity CurrentFacility(const entt::registry& registry, entt::entity shell) {
    if (shell == entt::null || registry.all_of<Destroyed>(shell)) {
        return entt::null;
    }
    const FacilityRef* facility = registry.try_get<FacilityRef>(shell);
    if (facility == nullptr || facility->kind != FacilityKind::Repair) {
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

struct SectionLayout {
    Rectangle panel{};   // The bracket-bordered box wrapping the rest of this section.
    Rectangle label{};   // Subject name + "Worst first"-style hint, inside panel.
    Rectangle list{};    // The ListView content rect -- what Update() hit-tests against.
    Rectangle footer{};  // Cost-to-full text plus the REPAIR ALL/STOP button, inside panel.
    Rectangle hint{};    // "Destroyed hardpoints can't be ordered here..." -- blank unless needed.
};

struct Layout {
    Rectangle header{};
    std::vector<SectionLayout> sections;  // One or two, laid out side by side.
};

// The button half of `footer` -- Update() hit-tests against exactly this, Draw() renders
// DrawChamferedButton into it; the rest of `footer` is the cost/status text, left-aligned.
Rectangle FooterButtonRect(Rectangle footer) {
    return {footer.x + footer.width - kFooterButtonWidth, footer.y, kFooterButtonWidth,
            footer.height};
}

// `content` is bridge_view::FrameContentRect() -- already the router's one full-screen inset, so
// this lays sections out inside it directly rather than re-insetting via sr::ui::PanelContentRect,
// except for each panel's own interior, which gets exactly one nested inset (each subject is
// framed as its own sub-panel, per issue #226's reference -- Storage's own two-column layout,
// generalised down to one column when the station is not the player's own).
Layout ComputeLayout(Rectangle content, int sectionCount) {
    Layout layout;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    const float y = content.y + kHeaderHeight + kSectionGap;
    const float columnWidth =
        sectionCount <= 1 ? content.width : (content.width - kColumnGap) * 0.5f;
    const float panelHeight = kLabelHeight + kListHeight + kFooterGap + kFooterHeight + kHintGap +
                              kHintHeight + sr::ui::kPanelPadding * 2.0f;

    for (int i = 0; i < sectionCount; ++i) {
        SectionLayout section;
        const float x = content.x + static_cast<float>(i) * (columnWidth + kColumnGap);
        section.panel = {x, y, columnWidth, panelHeight};
        const Rectangle inner = sr::ui::PanelContentRect(section.panel);
        section.label = {inner.x, inner.y, inner.width, kLabelHeight};
        section.list = {inner.x, inner.y + kLabelHeight, inner.width, kListHeight};
        section.footer = {inner.x, inner.y + kLabelHeight + kListHeight + kFooterGap, inner.width,
                          kFooterHeight};
        section.hint = {inner.x, section.footer.y + kFooterHeight + kHintGap, inner.width,
                        kHintHeight};
        layout.sections.push_back(section);
    }
    return layout;
}

}  // namespace

std::vector<RepairRow> Rows(const entt::registry& registry, entt::entity subject, bool hasOrder,
                            entt::entity orderedHardpoint, int costPerHp) {
    std::vector<RepairRow> rows;
    const Rig* rig = registry.try_get<Rig>(subject);
    if (rig == nullptr) {
        return rows;
    }
    for (const entt::entity hardpoint : rig->children) {
        const Health* health = registry.try_get<Health>(hardpoint);
        if (health == nullptr) {
            continue;
        }
        RepairRow entry;
        entry.hardpoint = hardpoint;
        entry.destroyed = registry.all_of<Destroyed>(hardpoint);
        entry.ordered = !entry.destroyed && hasOrder &&
                        (orderedHardpoint == entt::null || orderedHardpoint == hardpoint);

        if (const ShellRole* role = registry.try_get<ShellRole>(hardpoint)) {
            ShellGlyph(role->kind, entry.row.glyph);
        }
        if (const MountRef* mount = registry.try_get<MountRef>(hardpoint)) {
            entry.row.label = mount->id.str();
        }
        entry.row.style.integrity = health->max > 0.0f ? health->current / health->max : 0.0f;
        entry.row.style.disabled = entry.destroyed;

        const std::string hull = std::to_string(static_cast<int>(health->current)) + "/" +
                                 std::to_string(static_cast<int>(health->max));
        if (entry.destroyed) {
            entry.row.value = hull + "  REBUILD (P4-05)";
        } else {
            const float missing = std::max(0.0f, health->max - health->current);
            entry.costToFull = static_cast<int>(std::ceil(missing * static_cast<float>(costPerHp)));
            entry.row.value = hull + "  " + std::to_string(entry.costToFull) + "cr";
            if (entry.ordered) {
                entry.row.value += "  [STOP]";
            }
        }
        rows.push_back(std::move(entry));
    }
    // architecture.md 12.30.4: "the thing most likely to kill you is the first row."
    std::sort(rows.begin(), rows.end(), [](const RepairRow& a, const RepairRow& b) {
        return a.row.style.integrity < b.row.style.integrity;
    });
    return rows;
}

bool RepairAllActive(bool hasOrder, entt::entity orderedHardpoint) {
    return hasOrder && orderedHardpoint == entt::null;
}

int FacilityGrade(const entt::registry& registry, entt::entity facilityHardpoint) {
    const FacilityRef* facility = registry.try_get<FacilityRef>(facilityHardpoint);
    return facility != nullptr ? facility->grade : 1;
}

float FacilityRate(const entt::registry& registry, const core::ContentLibrary& content,
                   entt::entity facilityHardpoint) {
    const MountedModules* mounted = registry.try_get<MountedModules>(facilityHardpoint);
    if (mounted == nullptr) {
        return 0.0f;
    }
    float rate = 0.0f;
    for (const ModuleId& id : mounted->ids) {
        const ModuleDef* module = content.FindModule(id);
        if (module != nullptr && module->facility.kind == FacilityKind::Repair) {
            rate = module->facility.ratePerSecond;
            break;
        }
    }
    if (rate <= 0.0f) {
        return 0.0f;
    }
    if (const ParentRig* parent = registry.try_get<ParentRig>(facilityHardpoint)) {
        if (const CrewRepairBonus* bonus = registry.try_get<CrewRepairBonus>(parent->root)) {
            rate *= (1.0f + bonus->value);
        }
    }
    return rate;
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

namespace {

// The valid subjects for this screen, in display order: the requester's own vessel first, then
// the station itself when its FactionRef is the player's own (architecture.md 12.30.3's
// ownership test).
std::vector<entt::entity> Subjects(const entt::registry& registry, entt::entity station,
                                   entt::entity requester, const FactionId& playerFaction) {
    std::vector<entt::entity> subjects{requester};
    if (const FactionRef* stationFaction = registry.try_get<FactionRef>(station);
        stationFaction != nullptr && stationFaction->id == playerFaction) {
        subjects.push_back(station);
    }
    return subjects;
}

void ToggleOrder(entt::registry& registry, entt::entity requester, entt::entity subject,
                 entt::entity hardpoint, const RepairOrder* current) {
    const bool alreadyActive =
        current != nullptr && current->subject == subject && current->hardpoint == hardpoint;
    if (alreadyActive) {
        registry.remove<RepairOrder>(requester);
    } else {
        registry.emplace_or_replace<RepairOrder>(requester,
                                                 RepairOrder{subject, hardpoint, 1.0f, 0.0f});
    }
}

}  // namespace

void Update(entt::registry& registry, const FactionId& playerFaction) {
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

    const std::vector<entt::entity> subjects =
        Subjects(registry, station, requester, playerFaction);
    const Layout layout =
        ComputeLayout(bridge_view::FrameContentRect(), static_cast<int>(subjects.size()));
    const RepairOrder* order = registry.try_get<RepairOrder>(requester);
    const int costPerHp = core::economy::RepairCostPerHp(FacilityGrade(registry, facility));

    for (std::size_t i = 0; i < subjects.size(); ++i) {
        const entt::entity subject = subjects[i];
        const SectionLayout& section = layout.sections[i];
        const bool hasOrder = order != nullptr && order->subject == subject;

        if (sr::ui::ButtonClicked(FooterButtonRect(section.footer), input)) {
            ToggleOrder(registry, requester, subject, entt::null, order);
            return;
        }

        const std::vector<RepairRow> rows =
            Rows(registry, subject, hasOrder, hasOrder ? order->hardpoint : entt::null, costPerHp);
        const std::optional<int> hit =
            sr::ui::ListViewRowAt(section.list, static_cast<int>(rows.size()), 0.0f, input.cursor);
        if (hit.has_value() && *hit < static_cast<int>(rows.size())) {
            const RepairRow& row = rows[static_cast<std::size_t>(*hit)];
            if (!row.destroyed) {
                ToggleOrder(registry, requester, subject, row.hardpoint, order);
            }
            return;
        }
    }
}

namespace {

// architecture.md 2.2's function-length cap -- split out of Draw() below, one section each.

// Station name (large, the screen's own identity -- Bay/Storage's own precedent, and until
// bridge_view grows a top bar of its own the only place it is shown at all) over one consolidated
// GRADE/RATE/CREDITS stat line, the facility's own integrity gauge right-aligned on the title row
// (features.md 3.4's mandatory per-screen health readout).
void DrawHeader(Rectangle header, const std::string& stationName, int grade, float ratePerSecond,
                const std::optional<int>& credits, float facilityIntegrity) {
    DrawText(stationName.c_str(), static_cast<int>(header.x), static_cast<int>(header.y), 24,
             sr::ui::kValueBright);

    int x = static_cast<int>(header.x);
    const int y = static_cast<int>(header.y + 30.0f);
    auto DrawStat = [&](const std::string& label, const std::string& value) {
        DrawText(label.c_str(), x, y, 14, sr::ui::kLabelDim);
        x += MeasureText(label.c_str(), 14);
        DrawText(value.c_str(), x, y, 14, sr::ui::kValueBright);
        x += MeasureText(value.c_str(), 14);
        DrawText("   |   ", x, y, 14, sr::ui::kLabelDim);
        x += MeasureText("   |   ", 14);
    };
    DrawStat("GRADE ", std::to_string(grade));
    char rateBuf[16];
    std::snprintf(rateBuf, sizeof(rateBuf), "%.1f", static_cast<double>(ratePerSecond));
    DrawStat("RATE ", std::string(rateBuf) + " HP/S");
    if (credits.has_value()) {
        DrawText("CREDITS ", x, y, 14, sr::ui::kLabelDim);
        x += MeasureText("CREDITS ", 14);
        DrawText((std::to_string(*credits) + " CR").c_str(), x, y, 14, sr::ui::kValueBright);
    }

    const Color gaugeColor = facilityIntegrity > 0.5f   ? sr::ui::kStatusGood
                             : facilityIntegrity > 0.2f ? sr::ui::kStatusCaution
                                                        : sr::ui::kStatusCritical;
    const Rectangle gaugeBounds{header.x + header.width * 0.62f, header.y, header.width * 0.38f,
                                20.0f};
    sr::ui::DrawGauge(gaugeBounds, "FACILITY INTEGRITY", facilityIntegrity, gaugeColor);
}

// The reference's "VANGUARD -- YOUR VESSEL" / "Worst first" box: a bracket panel plus a label row
// naming this subject and a short hint, and (when at least one hardpoint here is destroyed) a
// dim reminder that a dead socket needs Rebuild elsewhere -- drawn around (but not including) the
// ListView and the REPAIR ALL/STOP footer.
void DrawSectionPanel(const SectionLayout& section, const std::string& title,
                      const std::string& hint, bool hasDestroyed) {
    sr::ui::DrawBracketPanel(section.panel, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f, 2.0f);
    DrawText(title.c_str(), static_cast<int>(section.label.x), static_cast<int>(section.label.y),
             14, sr::ui::kValueBright);
    const int hintWidth = MeasureText(hint.c_str(), 14);
    DrawText(hint.c_str(), static_cast<int>(section.label.x + section.label.width - hintWidth),
             static_cast<int>(section.label.y), 14, sr::ui::kLabelDim);
    if (hasDestroyed) {
        DrawText(
            "Destroyed hardpoints can't be ordered here -- Rebuild on the Engineering screen "
            "first.",
            static_cast<int>(section.hint.x), static_cast<int>(section.hint.y), 12,
            sr::ui::kLabelDim);
    }
}

// The footer row: "REPAIR ALL -- {cost} CR TO FULL" (left, priced from every non-destroyed row's
// RepairRow::costToFull -- 0 once the whole subject is at target) plus the REPAIR ALL/STOP button
// (right, RepairAllActive-driven -- the same toggle Update() already performs, unchanged by this
// pass).
void DrawFooter(const SectionLayout& section, const std::vector<RepairRow>& rows, bool allActive) {
    int totalCost = 0;
    for (const RepairRow& row : rows) {
        if (!row.destroyed) {
            totalCost += row.costToFull;
        }
    }
    const std::string costLabel = "REPAIR ALL -- " + std::to_string(totalCost) + " CR TO FULL";
    DrawText(costLabel.c_str(), static_cast<int>(section.footer.x),
             static_cast<int>(section.footer.y + section.footer.height * 0.5f - 7.0f), 14,
             sr::ui::kLabelDim);

    const Font font = GetFontDefault();
    sr::ui::DrawChamferedButton(FooterButtonRect(section.footer), allActive ? "STOP" : "REPAIR",
                                font, 14.0f, sr::ui::kPanelGlass, sr::ui::kPanelChrome,
                                sr::ui::kValueBright);
}

}  // namespace

void Draw(const entt::registry& registry, const FactionId& playerFaction,
          const core::ContentLibrary& content) {
    const entt::entity shell = PlayerShell(registry);
    const entt::entity facility = CurrentFacility(registry, shell);
    if (facility == entt::null) {
        return;
    }
    const entt::entity station = registry.get<ParentRig>(facility).root;
    const entt::entity requester = OwnedVesselAt(registry, station, playerFaction);
    if (requester == entt::null) {
        return;  // No owned hull docked here to repair.
    }

    const std::vector<entt::entity> subjects =
        Subjects(registry, station, requester, playerFaction);
    const Layout layout =
        ComputeLayout(bridge_view::FrameContentRect(), static_cast<int>(subjects.size()));

    std::string stationName = "REPAIR BAY";
    if (const DisplayName* name = registry.try_get<DisplayName>(station)) {
        stationName = name->value;
    }
    std::string vesselName = "VESSEL";
    if (const DisplayName* name = registry.try_get<DisplayName>(requester)) {
        vesselName = name->value;
    }
    const Health* facilityHealth = registry.try_get<Health>(facility);
    const float facilityIntegrity = facilityHealth != nullptr && facilityHealth->max > 0.0f
                                        ? facilityHealth->current / facilityHealth->max
                                        : 1.0f;
    const int grade = FacilityGrade(registry, facility);
    const float rate = FacilityRate(registry, content, facility);
    std::optional<int> credits;
    if (const Wallet* wallet = registry.try_get<Wallet>(requester)) {
        credits = wallet->credits;
    }
    DrawHeader(layout.header, stationName, grade, rate, credits, facilityIntegrity);

    const RepairOrder* order = registry.try_get<RepairOrder>(requester);
    const int costPerHp = core::economy::RepairCostPerHp(grade);

    for (std::size_t i = 0; i < subjects.size(); ++i) {
        const entt::entity subject = subjects[i];
        const SectionLayout& section = layout.sections[i];
        const bool hasOrder = order != nullptr && order->subject == subject;
        const bool isVessel = subject == requester;

        const std::vector<RepairRow> rows =
            Rows(registry, subject, hasOrder, hasOrder ? order->hardpoint : entt::null, costPerHp);
        bool hasDestroyed = false;
        for (const RepairRow& row : rows) {
            hasDestroyed = hasDestroyed || row.destroyed;
        }

        const std::string title =
            isVessel ? vesselName + " -- YOUR VESSEL" : stationName + " -- THIS STATION";
        const std::string hint = isVessel ? "Worst first" : "Yours, so it repairs itself";
        DrawSectionPanel(section, title, hint, hasDestroyed);

        std::vector<sr::ui::Row> widgetRows;
        widgetRows.reserve(rows.size());
        for (const RepairRow& entry : rows) {
            widgetRows.push_back(entry.row);
        }
        sr::ui::DrawListView(section.list, widgetRows, 0.0f, "NOTHING TO REPAIR");

        const bool allActive = RepairAllActive(hasOrder, hasOrder ? order->hardpoint : entt::null);
        DrawFooter(section, rows, allActive);
    }
}

}  // namespace sr::space::ui::repair_screen
