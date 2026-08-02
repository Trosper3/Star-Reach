#pragma once

#include <cstdint>
#include <variant>

#include "shared/blueprints/Ids.h"
#include "shared/blueprints/ShipBlueprint.h"
#include "shared/math/Vec2.h"

// Law 9 -- the multiplayer authority discipline, which is NOT deferred even though ENet is.
//
// UI and input never mutate game state. They emit intents; systems consume them, validate
// against core/, and apply results. In single-player the queue is drained locally in the same
// frame, so this costs one indirection now and saves restructuring every menu later.
//
// The stable-id rule from Law 2 applies with full force here: an intent may be produced on one
// machine and consumed on another, so it carries NO entt::entity. It carries the ids and
// coordinates a system needs to re-resolve the target in its own registry.
namespace sr::core {

// A player or NPC actor, stable across the wire. Index 0 is reserved for "no actor".
struct ActorId {
    std::uint32_t value = 0;
    friend bool operator==(ActorId, ActorId) = default;
};

// Throttle and steering for this tick, each in [-1, 1].
struct SetThrottleIntent {
    ActorId actor;
    float forward = 0.0f;
    float strafe = 0.0f;
    float turn = 0.0f;
};

// Fire whatever weapon hardpoints the actor's rig currently has. Which turrets that resolves to
// is a property of the rig at consumption time, not of the intent -- so an intent produced
// before a turret was shot off simply fires fewer guns.
struct FireWeaponsIntent {
    ActorId actor;
};

// Select a rig, and optionally one of its hardpoints, to engage. Referenced by stable mount id
// rather than by handle, so a targeting order survives the trip over a wire.
struct SetTargetIntent {
    ActorId actor;
    std::uint64_t targetNetworkId = 0;
    MountId targetMount;
};

// Instantiate a blueprint. The macro-loop payoff of Law 3: this is a request to build *data*,
// which is why a faction can manufacture a player's Template indefinitely.
struct SpawnBlueprintIntent {
    ActorId actor;
    BlueprintId blueprint;
    FactionId faction;
    Vec2 position;
    float rotation = 0.0f;
};

// Save a finished, already-validated Template draft into the actor's own knowledge network
// (architecture.md 12.9, features.md 2.2-2.3). CustomizeMenu (modes/space/ui/) emits this on
// Save rather than calling KnowledgeStore::Grant directly -- Law 9's "UI never mutates game
// state" holds even though the consumer is a plain store call, not a system tick.
struct SaveTemplateIntent {
    ActorId actor;
    KnowledgeNetworkId targetNetwork;
    ShipBlueprint blueprint;
};

using Intent = std::variant<SetThrottleIntent, FireWeaponsIntent, SetTargetIntent,
                            SpawnBlueprintIntent, SaveTemplateIntent>;

}  // namespace sr::core
