#pragma once

#include <string>

#include "shared/math/Vec2.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// Set by input or AI; consumed and cleared by WarpSystem the same tick, the same idiom as
// Combat.h's FireIntent. Local warp only (architecture.md section 4) -- an instant, same-
// registry reposition. Legacy StarReach2's BeginLocalWarp (its only caller was the galaxy map's
// in-system "warp to" click) charged no fuel and enforced no range limit, so neither does this.
struct WarpRequest {
    Vec2 targetPosition;
};

// Set by input or AI; consumed by SpaceFlight, NOT WarpSystem. A system-level handoff (tear down
// this SystemWorld, stand up `targetSystemId`'s) can only happen where SystemContext's no-mode-
// pointer boundary doesn't apply -- SpaceFlight.cpp is the only place that is (Law 6/7). No
// player-facing trigger exists yet (no galaxy map/destination picker), so this is set only by
// tests until one lands (architecture.md section 12.5's follow-up).
struct SystemWarpRequest {
    std::string targetSystemId;
    Vec2 spawnPosition;
    float spawnRotation = 0.0f;
};

}  // namespace sr
