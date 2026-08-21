#pragma once

#include <entt/entity/registry.hpp>
#include <span>
#include <string_view>
#include <vector>

#include "shared/blueprints/Taxonomy.h"

// modes/space/ui/ -- BridgeView, the docked-menu router (architecture.md section 12.24 step 5,
// section 12.30). features.md section 4's "component-driven menus": "the Bridge UI generates
// from physical modules... destroying a hardpoint removes its tab mid-session".
//
// features.md section 4 as a whole describes a much larger RTS fleet-command mode (macro-
// commands, AI directive autonomy) that is explicitly 📋 with open design questions still
// unresolved in the design doc itself -- not buildable yet, and not what this issue scopes.
// What IS buildable now is the piece that quote actually describes: which tabs exist is a
// direct, live function of which FacilityRef hardpoints the docked station has and has not lost
// (shared/components/Facility.h, wired in RigFactory.cpp's AttachModule), plus a Storage tab
// keyed on the host simply carrying a CargoHold (architecture.md 12.30.3 -- no facility
// hardpoint at all). No tab has any screen CONTENT behind it yet -- each is its own future issue
// (architecture.md 12.30.2-.8); this is the tab list, selection, and the PlayerLocation write
// selecting one performs.
namespace sr::space::ui::bridge_view {

// One entry in the router's tab strip. Every screen but Storage is exactly one FacilityKind
// (architecture.md 12.30's screen inventory); Storage has no facility hardpoint at all, so
// `hardpoint` is entt::null for it -- there is nothing to move PlayerLocation onto, and its
// per-screen readout measures the station's aggregate integrity instead of one hardpoint's.
enum class ScreenId : std::uint8_t {
    Bay,           // FacilityKind::Docking
    Market,        // FacilityKind::Trade
    Storage,       // No FacilityKind -- gated on the host carrying a CargoHold (12.30.3)
    Repair,        // FacilityKind::Repair
    Engineering,   // FacilityKind::Engineering
    Manufacturing, // FacilityKind::Manufacturing
    Research,      // FacilityKind::Research
};

std::string_view ToString(ScreenId value);

struct BridgeTab {
    ScreenId screen = ScreenId::Bay;
    // The hardpoint selecting this tab moves PlayerLocation onto (architecture.md 12.30's tab-
    // list fix). entt::null for Storage -- see ScreenId's comment above.
    entt::entity hardpoint = entt::null;
};

// Distinct screens available among `rigRoot`'s live (non-Destroyed) hardpoints, in ScreenId
// declaration order -- first living hardpoint of each FacilityKind, deduped. Pure -- no raylib --
// so unit-testable. Empty for a rig with no Facility hardpoints and no CargoHold, and for
// entt::null.
std::vector<BridgeTab> AvailableTabs(const entt::registry& registry, entt::entity rigRoot);

// Pure -- the tab-selection half of the router (architecture.md 12.30.1: "selecting a tab is
// moving into that hardpoint"). Moves PlayerLocation from `shell` onto `tabs[tabIndex].hardpoint`
// and nothing else; PlayerControlled is derived elsewhere (modes/space/systems/
// PlayerLocationSystem.h), never written here. No-op when `tabIndex` is out of range, when that
// tab's hardpoint is entt::null (Storage -- nothing physical to stand in), or when it already
// names `shell`.
void SelectTab(entt::registry& registry, entt::entity shell, std::span<const BridgeTab> tabs,
              int tabIndex);

// Reads this frame's input (raylib) and the PlayerControlled entity's Docked state: hit-tests the
// tab strip Draw() below renders and, on a click, calls SelectTab. No-op with no PlayerControlled
// entity, or one that is not Docked.
void Update(entt::registry& registry);

// Draws the docked station's tab strip, screen-space, centered -- only when the PlayerControlled
// entity is currently Docked. No-op otherwise.
void Draw(const entt::registry& registry);

}  // namespace sr::space::ui::bridge_view
