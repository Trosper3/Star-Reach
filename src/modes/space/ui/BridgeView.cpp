#include "modes/space/ui/BridgeView.h"

#include <raylib.h>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
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

constexpr float kPanelWidth = 520.0f;
constexpr float kPanelTop = 80.0f;
constexpr float kPanelHeight = 44.0f;

Rectangle PanelBounds() {
    const float screenWidth = static_cast<float>(GetScreenWidth());
    return Rectangle{(screenWidth - kPanelWidth) * 0.5f, kPanelTop, kPanelWidth, kPanelHeight};
}

// The entity currently carrying PlayerLocation (architecture.md 12.30.1 -- exactly one per
// registry), or entt::null if OnEnter hasn't placed a player.
entt::entity PlayerShell(const entt::registry& registry) {
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        return entity;
    }
    return entt::null;
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
    const entt::entity hardpoint = tabs[static_cast<std::size_t>(tabIndex)].hardpoint;
    if (hardpoint == entt::null || hardpoint == shell) {
        return;
    }
    registry.remove<PlayerLocation>(shell);
    registry.emplace<PlayerLocation>(hardpoint, PlayerLocation{hardpoint});
}

void Update(entt::registry& registry) {
    const entt::entity station = DockedStation(registry);
    if (station == entt::null) {
        return;
    }

    const std::vector<BridgeTab> tabs = AvailableTabs(registry, station);
    const sr::ui::UiInput input{GetMousePosition(), IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                GetMouseWheelMove()};
    if (!input.clicked) {
        return;
    }

    const Rectangle content = sr::ui::PanelContentRect(PanelBounds());
    const std::optional<int> hit =
        sr::ui::TabStripHitTest(content, static_cast<int>(tabs.size()), input.cursor);
    if (!hit.has_value()) {
        return;
    }

    const entt::entity shell = PlayerShell(registry);
    if (shell == entt::null) {
        return;
    }
    SelectTab(registry, shell, tabs, *hit);
}

void Draw(const entt::registry& registry) {
    const entt::entity station = DockedStation(registry);
    if (station == entt::null) {
        return;
    }

    const std::vector<BridgeTab> tabs = AvailableTabs(registry, station);
    const Rectangle content = sr::ui::DrawPanelFrame(PanelBounds());

    std::vector<std::string> labels;
    labels.reserve(tabs.size());
    for (const BridgeTab& tab : tabs) {
        labels.emplace_back(ToString(tab.screen));
    }

    const entt::entity shell = PlayerShell(registry);
    int selected = -1;
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        if (tabs[i].hardpoint != entt::null && tabs[i].hardpoint == shell) {
            selected = static_cast<int>(i);
            break;
        }
    }

    sr::ui::DrawTabStrip(content, labels, selected);
}

}  // namespace sr::space::ui::bridge_view
