#pragma once

#include "shared/math/Vec2.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// Ported verbatim from legacy StarReach2's TutorialStep. Each step completes at its own action
// site; TutorialSystem does not know which step is "next" beyond incrementing the enum.
enum class TutorialStep : unsigned char {
    Move,
    Target,
    DestroyAsteroid,
    CollectElement,
    Dock,
    Sell,
    EquipModule,
    Warp,
    Done,
};

// Tag + state: this rig is progressing through the tutorial. Presence of this component IS
// "tutorial active" -- there is no separate active flag, and removing it (or never adding it) is
// how skipping/not-taking-the-tutorial is represented.
struct Tutorial {
    TutorialStep step = TutorialStep::Move;
    // Captured lazily by TutorialSystem the first tick it sees this component, from whatever
    // WorldTransform.position the rig has then -- callers starting a tutorial do not need to
    // know the rig's position up front.
    Vec2 startPosition;
    bool started = false;
};

}  // namespace sr
