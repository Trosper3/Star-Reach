#pragma once

#include <entt/entity/registry.hpp>
#include <vector>

#include "shared/blueprints/Taxonomy.h"

// modes/space/ui/ -- BridgeView (architecture.md section 3; features.md section 4's "component-
// driven menus": "the Bridge UI generates from physical modules... destroying a hardpoint
// removes its tab mid-session").
//
// features.md section 4 as a whole describes a much larger RTS fleet-command mode (macro-
// commands, AI directive autonomy) that is explicitly 📋 with open design questions still
// unresolved in the design doc itself -- not buildable yet, and not what this issue scopes.
// What IS buildable now is the piece that quote actually describes: which tabs exist is a
// direct, live function of which FacilityRef hardpoints the docked station has and has not lost
// (shared/components/Facility.h, wired in RigFactory.cpp's AttachModule in this same commit).
// No tab has any CONTENT behind it yet -- Repair/Manufacturing/Research/Storage are each their
// own future system; this is the tab list only.
namespace sr::space::ui::bridge_view {

// Distinct FacilityKinds present among `rigRoot`'s live (non-Destroyed) hardpoints, in
// FacilityKind declaration order. Pure -- no raylib -- so unit-testable. Empty for a rig with no
// Facility hardpoints, and for entt::null.
std::vector<FacilityKind> AvailableTabs(const entt::registry& registry, entt::entity rigRoot);

// Draws the docked station's tab list, screen-space, centered -- only when the PlayerControlled
// entity is currently Docked. No-op otherwise.
void Draw(const entt::registry& registry);

}  // namespace sr::space::ui::bridge_view
