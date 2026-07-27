#pragma once

#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/factories/RigFactory.h"

namespace sr::space::npc_factory {

// The opposition's entry point into Law 3's blueprint->entity pipeline.
//
// RigFactory::Spawn is THE converter from blueprint form to live form -- "nothing else converts
// between them" (RigFactory.h). This does not duplicate that conversion; it delegates to it. What
// it gives callers (WorldGen, SpawnSystem, once they land) is a named seam for opposition
// specifically, so "what makes an entity an NPC" stays out of systems/ and out of RigFactory, and
// so the Patrol/Chase/Attack/Flee/Escort behaviour this slice defers (architecture.md section 10,
// item 7) has one place to attach its setup later without touching RigFactory or its callers.
//
// A spawned NPC carries no PlayerControlled tag -- RigFactory never adds one -- which is what
// NpcAiSystem filters on to find the rigs it drives.
using SpawnParams = rig_factory::SpawnParams;
using SpawnResult = rig_factory::SpawnResult;

SpawnResult Spawn(SystemWorld& world, const core::ContentLibrary& content,
                  const SpawnParams& params);

}  // namespace sr::space::npc_factory
