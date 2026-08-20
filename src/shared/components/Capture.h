#pragma once

#include <entt/entity/entity.hpp>

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// On a rig root actively holding position beside a boardable target -- features.md 3.2's
// "boarding-in-place," settled 2026-08-11. Written and cleared entirely by CaptureSystem: an
// interrupted hold (the target leaves range, dies, or a new target is found) resets `heldSeconds`
// rather than pausing it, the same "exposure-for-duration" trade every other stand-still-and-
// take-the-risk mechanic in this design uses.
struct BoardingHold {
    entt::entity target = entt::null;
    float heldSeconds = 0.0f;
};

}  // namespace sr
