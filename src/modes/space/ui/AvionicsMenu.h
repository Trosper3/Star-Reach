#pragma once

#include <entt/entity/registry.hpp>

// modes/space/ui/ -- AvionicsMenu (architecture.md section 3).
//
// Handles the one navigation action the game currently has an end-to-end path for: docking.
// Per section 2.3's layer rule, modes/*/ui/ must not include systems/ and per Law 9 must never
// mutate game state directly -- Update() satisfies both by writing DockRequest/UndockRequest,
// components DockingSystem (already built, in systems/) already consumes. That is this
// codebase's established intent-emission idiom: FactionEconomySystem's DepositRequest/
// SpendRequest and DockingSystem's own DockRequest/UndockRequest all predate a UI producer the
// same way ("Set by input or AI" is each one's own doc comment) -- this file is that producer
// for docking, not a new pattern.
namespace sr::space::ui::avionics_menu {

// Reads this frame's input (raylib) and the PlayerControlled entity's DockPrompt/Docked state:
// pressing the dock key while DockPrompt names a bay writes a DockRequest for that bay; pressing
// it while Docked writes an UndockRequest. No-op with no PlayerControlled entity, or when
// neither state applies.
void Update(entt::registry& registry);

// "[E] DOCK" / "[E] UNDOCK" prompt text, screen-space, above CockpitHud's hull bar. No-op when
// neither DockPrompt nor Docked applies to the player.
void Draw(const entt::registry& registry);

}  // namespace sr::space::ui::avionics_menu
