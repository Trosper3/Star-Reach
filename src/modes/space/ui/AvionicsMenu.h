#pragma once

#include <entt/entity/registry.hpp>

#include "shared/blueprints/Ids.h"

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
//   - Board and launch (architecture.md 12.30.2): pressing the dock key while standing inside a
//     facility hardpoint (PlayerLocation names one, so the derived PlayerControlled is the
//     station, not Docked -- a naive UndockRequest there would do nothing) resolves the player's
//     own vessel docked at that station (modes/space/ui/BayView.h's OwnedVesselAt) and performs
//     both writes behind the one key: PlayerLocation moves onto its cockpit, then UndockRequest
//     lands on it. No-op with no owned hull docked there.
//   - Power (features.md 2.9): tapping a category key (F/G/H/J -- weapons/shields/engines/
//     facilities) toggles that category Boosted; holding it past a short threshold toggles it
//     Offline instead; Shift+tap moves the category to the end of PowerPriorityList's shed order
//     (protects it, sheds last). Holding Ctrl forces engines Boosted for as long as it is held --
//     the momentary afterburner, "two affordances onto one state" alongside H's sustained toggle.
// No-op with no PlayerLocation entity. `playerFaction` (modes/space/systems/PlayerRecordSystem.h)
// is threaded in by the caller rather than looked up here -- modes/*/ui/ may not include systems/.
void Update(entt::registry& registry, const FactionId& playerFaction);

// "[R] DOCK" / "[R] UNDOCK" / "[R] LAUNCH" prompt text, screen-space, above CockpitHud's hull bar.
// No-op when none of DockPrompt, Docked, or an owned hull docked at the station PlayerLocation
// currently names applies to the player.
void Draw(const entt::registry& registry, const FactionId& playerFaction);

}  // namespace sr::space::ui::avionics_menu
