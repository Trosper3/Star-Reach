#include "modes/space/ui/BridgeView.h"

#include <raylib.h>
#include <algorithm>
#include <cstddef>
#include <string>

#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::bridge_view {
namespace {

// Declaration order (Taxonomy.h), not discovery order, so the tab list does not reshuffle as
// hardpoints are destroyed and rebuilt across a session.
constexpr FacilityKind kAllKinds[] = {
    FacilityKind::Repair,  FacilityKind::Manufacturing, FacilityKind::Research,
    FacilityKind::Docking, FacilityKind::Storage,       FacilityKind::Engineering,
};
// Catches the next FacilityKind addition at compile time instead of silently missing a tab
// (architecture.md 13.5 group 2: this list already fell one enumerator behind once).
static_assert(sizeof(kAllKinds) / sizeof(kAllKinds[0]) ==
                  static_cast<std::size_t>(FacilityKind::Engineering) + 1,
              "kAllKinds must list every FacilityKind enumerator");

constexpr float kPanelWidth = 360.0f;
constexpr float kPanelTop = 80.0f;
constexpr float kHeaderHeight = 40.0f;
constexpr float kRowHeight = 28.0f;
constexpr float kTitleFontSize = 20.0f;
constexpr float kRowFontSize = 16.0f;

}  // namespace

std::vector<FacilityKind> AvailableTabs(const entt::registry& registry, entt::entity rigRoot) {
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return {};
    }

    std::vector<FacilityKind> present;
    for (const entt::entity child : rig->children) {
        if (registry.all_of<Destroyed>(child)) {
            continue;
        }
        if (const FacilityRef* facility = registry.try_get<FacilityRef>(child)) {
            present.push_back(facility->kind);
        }
    }

    std::vector<FacilityKind> tabs;
    for (FacilityKind kind : kAllKinds) {
        if (std::find(present.begin(), present.end(), kind) != present.end()) {
            tabs.push_back(kind);
        }
    }
    return tabs;
}

void Draw(const entt::registry& registry) {
    for (auto [entity] : registry.view<PlayerControlled>().each()) {
        const Docked* docked = registry.try_get<Docked>(entity);
        if (docked == nullptr) {
            return;
        }

        const std::vector<FacilityKind> tabs = AvailableTabs(registry, docked->station);
        const float screenWidth = static_cast<float>(GetScreenWidth());
        const float panelHeight = kHeaderHeight + kRowHeight * static_cast<float>(tabs.size());
        const Rectangle bounds{(screenWidth - kPanelWidth) * 0.5f, kPanelTop, kPanelWidth,
                               panelHeight};

        sr::ui::DrawBracketPanel(bounds, sr::ui::kPanelGlass, sr::ui::kPanelChrome);

        const Font font = GetFontDefault();
        DrawTextEx(font, "BRIDGE", {bounds.x + 16.0f, bounds.y + 10.0f}, kTitleFontSize, 1.0f,
                   sr::ui::kValueBright);

        float rowY = bounds.y + kHeaderHeight;
        for (FacilityKind kind : tabs) {
            const std::string name(ToString(kind));
            DrawTextEx(font, name.c_str(), {bounds.x + 24.0f, rowY}, kRowFontSize, 1.0f,
                       sr::ui::kLabelDim);
            rowY += kRowHeight;
        }
        return;  // Exactly one PlayerControlled entity.
    }
}

}  // namespace sr::space::ui::bridge_view
