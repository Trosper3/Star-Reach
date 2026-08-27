#pragma once

#include <entt/entity/registry.hpp>

// modes/space/ui/ -- the prologue's lesson surface (features.md 1.2, issue #238's "the fixed
// authored system... modes/space/ui/ (the lesson surface)").
//
// TutorialSystem.cpp is the only producer of TutorialOffer (shared/components/Tutorial.h) and the
// only reader of AnomalyField -- this file is TutorialOffer's only consumer, and the first reader
// of Tutorial's own step for anything other than advancing it. Same split as CommsPanel.h's own
// header comment: read raylib, mutate the component directly, never touch simulation state that
// belongs to a system. Tutorial.h's own doc comment sanctions exactly this ("there is no separate
// start/skip request; add the component to begin, remove it to abandon").
namespace sr::space::ui::tutorial_hud {

// Pressing Y while the PlayerControlled rig carries TutorialOffer emplaces Tutorial on it and
// removes TutorialOffer (Accept); pressing N removes TutorialOffer and emplaces TutorialDeclined
// instead (Decline, "changes nothing else" -- features.md 1.2). No-op with no PlayerControlled
// entity or no pending offer.
void Update(entt::registry& registry);

// Draws the offer prompt while TutorialOffer is pending, and the current lesson's instruction
// banner while Tutorial is active and its step is not yet Done. Screen-space, top-center --
// CommsPanel occupies top-left and CockpitHud's status projection/slot bar occupy the bottom
// corners (architecture.md 12.30.7) -- so must be called outside BeginMode2D/EndMode2D, same as
// both. No-op with no PlayerControlled entity.
void Draw(const entt::registry& registry);

}  // namespace sr::space::ui::tutorial_hud
