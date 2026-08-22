#include "modes/space/ui/RepairScreen.h"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

#include "core/economy/Pricing.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/components/StationServices.h"
#include "shared/ui/HudTheme.h"
#include "shared/ui/UiInput.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::repair_screen {
namespace {

constexpr float kPanelWidth = 560.0f;
constexpr float kPanelTop = 132.0f;
constexpr float kHeaderHeight = 44.0f;
constexpr float kSectionLabelHeight = 20.0f;
constexpr float kListHeight = 160.0f;
constexpr float kAllButtonHeight = 24.0f;

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
    return shell;
}

struct SectionLayout {
    Rectangle label{};
    Rectangle list{};
    Rectangle allButton{};
};

struct Layout {
    Rectangle header{};
    std::vector<SectionLayout> sections;
};

Rectangle PanelBounds(int sectionCount) {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    const float sectionHeight = kSectionLabelHeight + kListHeight + kAllButtonHeight;
    const float height = kHeaderHeight + sectionHeight * static_cast<float>(sectionCount) +
                         2.0f * sr::ui::kPanelPadding;
    return Rectangle{(screenWidth - kPanelWidth) * 0.5f, kPanelTop, kPanelWidth, height};
}

Layout ComputeLayout(int sectionCount) {
    const Rectangle content = sr::ui::PanelContentRect(PanelBounds(sectionCount));
    Layout layout;
    layout.header = {content.x, content.y, content.width, kHeaderHeight};
    float y = content.y + kHeaderHeight;
    for (int i = 0; i < sectionCount; ++i) {
        SectionLayout section;
        section.label = {content.x, y, content.width, kSectionLabelHeight};
        y += kSectionLabelHeight;
        section.list = {content.x, y, content.width, kListHeight};
        y += kListHeight;
        section.allButton = {content.x, y, content.width, kAllButtonHeight};
        y += kAllButtonHeight;
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
            const int costToFull =
                static_cast<int>(std::ceil(missing * static_cast<float>(costPerHp)));
            entry.row.value = hull + "  " + std::to_string(costToFull) + "cr";
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
    const Layout layout = ComputeLayout(static_cast<int>(subjects.size()));
    const RepairOrder* order = registry.try_get<RepairOrder>(requester);
    const int costPerHp = core::economy::RepairCostPerHp(FacilityGrade(registry, facility));

    for (std::size_t i = 0; i < subjects.size(); ++i) {
        const entt::entity subject = subjects[i];
        const SectionLayout& section = layout.sections[i];
        const bool hasOrder = order != nullptr && order->subject == subject;

        if (sr::ui::ButtonClicked(section.allButton, input)) {
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

void Draw(const entt::registry& registry, const FactionId& playerFaction) {
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
    const Layout layout = ComputeLayout(static_cast<int>(subjects.size()));
    sr::ui::DrawPanelFrame(PanelBounds(static_cast<int>(subjects.size())));

    std::string facilityName = "REPAIR BAY";
    if (const DisplayName* name = registry.try_get<DisplayName>(station)) {
        facilityName = name->value;
    }
    const Health* facilityHealth = registry.try_get<Health>(facility);
    const float facilityIntegrity = facilityHealth != nullptr && facilityHealth->max > 0.0f
                                        ? facilityHealth->current / facilityHealth->max
                                        : 1.0f;
    DrawText(facilityName.c_str(), static_cast<int>(layout.header.x),
             static_cast<int>(layout.header.y), 18, sr::ui::kValueBright);
    const Color gaugeColor = facilityIntegrity > 0.5f   ? sr::ui::kStatusGood
                             : facilityIntegrity > 0.2f ? sr::ui::kStatusCaution
                                                        : sr::ui::kStatusCritical;
    const Rectangle gaugeBounds{layout.header.x + layout.header.width * 0.5f, layout.header.y,
                                layout.header.width * 0.5f, 20.0f};
    sr::ui::DrawGauge(gaugeBounds, "FACILITY INTEGRITY", facilityIntegrity, gaugeColor);

    const RepairOrder* order = registry.try_get<RepairOrder>(requester);
    const int costPerHp = core::economy::RepairCostPerHp(FacilityGrade(registry, facility));

    for (std::size_t i = 0; i < subjects.size(); ++i) {
        const entt::entity subject = subjects[i];
        const SectionLayout& section = layout.sections[i];
        const bool hasOrder = order != nullptr && order->subject == subject;

        const std::string subjectLabel = subject == requester ? "YOUR VESSEL" : "STATION";
        DrawText(subjectLabel.c_str(), static_cast<int>(section.label.x),
                 static_cast<int>(section.label.y), 14, sr::ui::kLabelDim);

        const std::vector<RepairRow> rows =
            Rows(registry, subject, hasOrder, hasOrder ? order->hardpoint : entt::null, costPerHp);
        std::vector<sr::ui::Row> widgetRows;
        widgetRows.reserve(rows.size());
        for (const RepairRow& entry : rows) {
            widgetRows.push_back(entry.row);
        }
        sr::ui::DrawListView(section.list, widgetRows, 0.0f, "NOTHING TO REPAIR");

        const bool allActive = RepairAllActive(hasOrder, hasOrder ? order->hardpoint : entt::null);
        const Font font = GetFontDefault();
        sr::ui::DrawChamferedButton(section.allButton, allActive ? "STOP" : "REPAIR ALL", font,
                                    14.0f, sr::ui::kPanelGlass, sr::ui::kPanelChrome,
                                    sr::ui::kValueBright);
    }
}

}  // namespace sr::space::ui::repair_screen
