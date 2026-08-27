#pragma once

#include "shared/math/Vec2.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// features.md 1.2: the prologue's two acts. Act I teaches Move/Target/Fire/Equip inside the
// fixed, hand-authored system P5-06 spawns; Act II's Gather/Dock-and-trade/Customize/Build/Warp
// lessons and its gathering tithe land with P6-13. Only Act::One is ever produced today -- P6-13
// is what gives Act::Two a producer.
enum class Act : unsigned char {
    One,
    Two,
};

// Re-scoped from legacy StarReach2's TutorialStep down to Act I's four lessons (features.md 1.2).
// DestroyAsteroid/CollectElement/Dock/Sell/Warp moved to Act II's list in P6-13 -- they taught
// gathering, docking/trade, and warping, none of which Act I's fixed system exercises. Each step
// completes at its own action site; TutorialSystem does not know which step is "next" beyond
// incrementing the enum.
enum class TutorialStep : unsigned char {
    Move,
    Target,
    Fire,
    Equip,
    Done,
};

// Tag + state: this rig is progressing through the tutorial. Presence of this component IS
// "tutorial active" -- there is no separate active flag, and removing it (or never adding it) is
// how skipping/not-taking-the-tutorial is represented. The prologue's commander offer
// (modes/space/ui/TutorialHud.h) adds this directly on Accept for exactly that reason.
struct Tutorial {
    Act act = Act::One;
    TutorialStep step = TutorialStep::Move;
    // Captured lazily by TutorialSystem the first tick it sees this component, from whatever
    // WorldTransform.position the rig has then -- callers starting a tutorial do not need to
    // know the rig's position up front.
    Vec2 startPosition;
    bool started = false;
    // Captured the same lazy way as startPosition, in the same first-tick block: how many
    // modules this rig already had mounted (summed across every hardpoint) before the tutorial
    // began. Equip advances once the live count exceeds this baseline -- every blueprint already
    // arrives with modules mounted, so "any hardpoint has MountedModules" would complete Equip
    // before the player did anything.
    int startEquippedModules = 0;
};

// Set on the player's rig root by TutorialSystem when the prologue's fleet commander hails on
// arrival (features.md 1.2), offering instruction. modes/space/ui/TutorialHud.h is the only
// reader/consumer: accepting emplaces Tutorial (see above) and removes this; declining emplaces
// TutorialDeclined instead. Never re-added once either happens -- TutorialSystem's offer check
// gates on both this and TutorialDeclined being absent.
struct TutorialOffer {};

// Tag: this rig was offered prologue instruction and declined. Declining "changes nothing else"
// (features.md 1.2) -- the fleet still flies and the anomaly still ends the act -- so this exists
// solely to stop TutorialSystem from re-offering every subsequent tick.
struct TutorialDeclined {};

}  // namespace sr
