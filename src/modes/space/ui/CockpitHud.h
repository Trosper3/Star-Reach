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

// Thin forward to shared/rig/ModuleAttachment.h's AggregateStructuralIntegrity -- the raw
// (un-normalized) sum of living current/max Health across a rig's hardpoints. There is no
// rig-wide health bar component (shared/components/Health.h: "a rig dies when its hardpoints
// do"), so this is the aggregate a HUD needs to show one bar; DamageSystem is the aggregate's
// other reader, which is why the computation itself lives in shared/rig/ rather than here
// (architecture.md 2.3: systems/ may not include ui/). Returns 0 for a rig with no Health-bearing
// hardpoints (a root with no children yet, not a crash). Pure -- no raylib -- so unit-testable.
float AggregateHullFraction(const entt::registry& registry, entt::entity rigRoot);

// Draws the PlayerControlled rig's hull bar, screen-space, bottom-left, normalized so it reaches
// a true zero at features.md 3.2's structural-failure threshold rather than at raw zero -- no
// ship is ever seen alive below that point. No-op with no PlayerControlled entity. Must be called
// outside BeginMode2D/EndMode2D, same as IconRenderer.
//
// architecture.md 12.30's frame: this is today's placeholder for features.md 3.10's bottom band,
// which the docked frame replaces rather than floats over ("the flight HUD does not persist under
// it") -- SpaceFlight::Draw skips this call while docked (modes/space/ui/BridgeView.h's
// DockedStation), rather than this function gating on it itself, the same split DrawWorld/
// DrawAimReticle already use.
void Draw(const entt::registry& registry);

}  // namespace sr::space::ui::cockpit_hud
