#pragma once

namespace sr {

// Tag: rigs are culled and respawned relative to the NEAREST entity carrying this, not one
// privileged global point -- typically the player and/or friendly stations. A system with no
// SpawnAnchor at all skips culling entirely (there is nothing yet to measure distance against),
// rather than treating "no anchor" as "infinitely far" and wiping the registry.
struct SpawnAnchor {};

// On a rig root that needs to reappear safely instead of at its last position -- e.g. the
// player's ship after death. SpawnSystem removes this once it has placed the rig, so a request
// resolves exactly once.
struct RespawnPending {
    // Minimum clearance from every other living rig's CollisionRadius, so the respawned rig does
    // not land overlapping something solid.
    float marginUnits = 80.0f;
};

}  // namespace sr
