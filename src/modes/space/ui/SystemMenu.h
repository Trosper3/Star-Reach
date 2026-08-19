#pragma once

#include <entt/entity/registry.hpp>

// modes/space/ui/ -- SystemMenu (architecture.md 12.29).
//
// The only pause in the game. features.md 3.4 forbids pausing on any surface that carries
// tactical value; this menu is legal precisely because it does not -- Resume and Quit only.
// Re-read architecture.md 12.29 before ever adding an entry: the power-priority list and the
// weapon-group editor are both explicitly excluded for carrying in-fight value.
//
// Open/closed state lives on a lazily-created singleton entity -- System.h's "one legitimate
// cache" exception (Law 6), the same CommsLog precedent (shared/components/Comms.h) -- not a
// SpaceFlight member.
namespace sr::space::ui::system_menu {

// Tag: the one entity per registry carrying SystemMenuState. Created lazily by Update() the
// first time Esc is pressed, not up front.
struct SystemMenuStateSingleton {};

struct SystemMenuState {
    bool open = false;
    // The Quit To Main Menu button's confirmation step: the first click arms it, swapping the
    // button row for "ALL PROGRESS IS LOST" plus Confirm/Cancel, rather than quitting outright.
    // There is no save yet (architecture.md 12.29), so this is the only guard against a stray
    // click discarding the run.
    bool quitArmed = false;
    // Set once Confirm is clicked. SpaceFlight::Update polls this once per frame -- the same
    // request/consume idiom SystemWarpRequest already establishes -- and never clears it itself;
    // the world (and this singleton with it) is destroyed by the transition either way.
    bool quitConfirmed = false;
};

// Reads this frame's input. The Esc ladder, innermost first (architecture.md 12.29):
//   1. Cancel build placement -- lands with P8-01; no such state exists yet.
//   2. Clear unit selection   -- lands with P8-02; no such state exists yet.
//   3. Open the menu, if closed.
//   4. Close the menu, if open (and disarm the quit confirmation with it).
// Insert rungs 1-2 above rung 3's check when those systems land, in this order -- do not
// reorder what is already here. While open, a left click also drives Resume/Quit/Confirm/Cancel.
void Update(entt::registry& registry);

// Draws the menu -- a centered bracket panel and its buttons -- when open; a no-op otherwise.
void Draw(const entt::registry& registry);

// True once SystemMenuState::open. SpaceFlight::Update reads this to pause: skip polling flight
// input and advancing FixedTimestep, so no real time banks in its accumulator while the player
// sits in the menu -- unpausing must not fast-forward the world.
bool IsOpen(const entt::registry& registry);

// True once Quit To Main Menu has been confirmed (SystemMenuState::quitConfirmed above).
bool QuitConfirmed(const entt::registry& registry);

}  // namespace sr::space::ui::system_menu
