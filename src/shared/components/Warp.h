#pragma once

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

}  // namespace sr
