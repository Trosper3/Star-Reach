#pragma once

#include <entt/entity/registry.hpp>

// modes/space/ui/ -- CommsPanel (architecture.md 13.3 finding S, 12.27; issue #235's "the comms
// surface").
//
// CommsLog is write-only today: CommsSystem formats a complete hail response and nothing reads
// it (architecture.md 13.3 S -- "the hail feature is complete from request to formatted response
// string, and the response is unreadable"). This file is the log's first reader and HailRequest's
// first producer outside a test -- the established Law 9 idiom (AvionicsMenu.h's DockRequest
// precedent): read raylib, emplace the request component, never touch simulation state directly.
namespace sr::space::ui::comms_panel {

// The nearest entity carrying WorldTransform + DisplayName + Rig (any relation, any distance)
// other than `player` -- entt::null if none. CommsSystem's own CommsRange check
// (shared/components/Comms.h) decides whether a hail aimed at it actually lands; this only names
// *something* to aim at, the same split AvionicsMenu leaves DockingSystem to police for docking.
// Pure -- no raylib -- so unit-testable.
entt::entity NearestHailable(const entt::registry& registry, entt::entity player);

// Reads this frame's raylib input: pressing the hail key while NearestHailable(the
// PlayerControlled rig root) names something writes a HailRequest for it on that root -- Law 9's
// established idiom (AvionicsMenu.h's DockRequest precedent). No-op with no PlayerControlled
// entity or nothing in range to name.
void Update(entt::registry& registry);

// Draws the CommsLog's up-to-8 entries as a bracket panel, hudtheme style -- the log's first
// reader (architecture.md 13.3 S) -- and, when NearestHailable names something, the hail prompt
// beneath it. Screen-space; must be called outside BeginMode2D/EndMode2D, same as CockpitHud.
// No-op with no PlayerControlled entity.
void Draw(const entt::registry& registry);

}  // namespace sr::space::ui::comms_panel
