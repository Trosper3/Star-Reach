#pragma once

#include <entt/entity/entity.hpp>

#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "shared/blueprints/Ids.h"
#include "shared/math/Vec2.h"

namespace sr::space::rig_factory {

// THE bridge between blueprint form and live form (Law 3). Nothing else converts between them.
//
// Blueprint in, entity graph out: one parent entity carrying Rig{children}, plus one child
// entity per mount carrying ParentRig, LocalTransform, WorldTransform, Health, and whatever role
// components its modules imply. A fighter, a station, and a capital all come out of this one
// function -- which is the whole of Law 4. StarReach2 built those three separately and ended up
// with four incompatible hardpoint wire formats.
//
// Object assembly is decoupled from simulation (Law 5): systems never construct rigs, and
// modes/space/systems/ is forbidden from including this header.

struct SpawnParams {
    BlueprintId blueprint;
    FactionId faction;  // Overrides the blueprint's authored faction when non-empty.
    Vec2 position;
    float rotation = 0.0f;
};

struct SpawnResult {
    entt::entity root = entt::null;
    NetworkId networkId = kInvalidNetworkId;

    bool ok() const { return root != entt::null; }
};

// Returns an invalid result if the blueprint is unknown or fails validation.
//
// Validation runs HERE, before any entity exists, so a blueprint that cannot fly never becomes
// a half-built rig that some system then has to defend against. That ordering is why no system
// downstream needs a "does this rig actually have an engine" check.
SpawnResult Spawn(SystemWorld& world, const core::ContentLibrary& content,
                  const SpawnParams& params);

// Resolves a rig's hardpoint by its stable mount id. The (NetworkId, MountId) pair is how a
// hardpoint is addressed across a save or the wire -- an entt::entity never is.
entt::entity FindHardpoint(const entt::registry& registry, entt::entity root, const MountId& mount);

}  // namespace sr::space::rig_factory
