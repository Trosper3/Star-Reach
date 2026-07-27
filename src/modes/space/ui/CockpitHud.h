#pragma once

#include <entt/entity/registry.hpp>

// modes/space/ui/ -- CockpitHud (architecture.md section 3), the first of the three files this
// issue adds alongside AvionicsMenu and BridgeView.
//
// Per section 2.3's layer rule, modes/*/ui/ must not include systems/ and must never mutate
// game state directly (Law 9) -- CockpitHud only reads, so that second rule is trivially
// satisfied here; AvionicsMenu.h documents how the one file in this trio that DOES write
// anything stays inside Law 9's discipline.
namespace sr::space::ui::cockpit_hud {

// Sum of current/max Health across a rig's hardpoints. There is no rig-wide health bar
// component (shared/components/Health.h: "a rig dies when its hardpoints do"), so this is the
// aggregate a HUD needs to show one bar. Returns 0 for a rig with no Health-bearing hardpoints
// (a root with no children yet, not a crash). Pure -- no raylib -- so unit-testable.
float AggregateHullFraction(const entt::registry& registry, entt::entity rigRoot);

// Draws the PlayerControlled rig's hull bar, screen-space, bottom-left. No-op with no
// PlayerControlled entity. Must be called outside BeginMode2D/EndMode2D, same as IconRenderer.
void Draw(const entt::registry& registry);

}  // namespace sr::space::ui::cockpit_hud
