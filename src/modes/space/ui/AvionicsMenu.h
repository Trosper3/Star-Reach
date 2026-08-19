#pragma once

#include <entt/entity/registry.hpp>

// modes/space/ui/ -- AvionicsMenu (architecture.md section 3).
//
// Docking, and power allocation (features.md 2.9, architecture.md 12.16 item 18).
// Per section 2.3's layer rule, modes/*/ui/ must not include systems/ and per Law 9 must never
// mutate game state directly -- Update() satisfies both by writing DockRequest/UndockRequest and
// PowerAllocation/PowerPriorityList directly, components DockingSystem/PowerSystem (both in
// systems/) already consume. That is this codebase's established intent-emission idiom:
// FactionEconomySystem's DepositRequest/SpendRequest and DockingSystem's own DockRequest/
// UndockRequest all predate a UI producer the same way ("Set by input or AI" is each one's own
// doc comment) -- this file is that producer, not a new pattern.
namespace sr::space::ui::avionics_menu {

// Reads this frame's input (raylib) and the PlayerLocation entity's state:
//   - Docking: pressing the dock key while DockPrompt names a bay writes a DockRequest for that
//     bay; pressing it while Docked writes an UndockRequest.
//   - Power (features.md 2.9): tapping a category key (F/G/H/J -- weapons/shields/engines/
//     facilities) toggles that category Boosted; holding it past a short threshold toggles it
//     Offline instead; Shift+tap moves the category to the end of PowerPriorityList's shed order
//     (protects it, sheds last). Holding Ctrl forces engines Boosted for as long as it is held --
//     the momentary afterburner, "two affordances onto one state" alongside H's sustained toggle.
// No-op with no PlayerLocation entity.
void Update(entt::registry& registry);

// "[R] DOCK" / "[R] UNDOCK" prompt text, screen-space, above CockpitHud's hull bar. No-op when
// neither DockPrompt nor Docked applies to the player.
void Draw(const entt::registry& registry);

}  // namespace sr::space::ui::avionics_menu
