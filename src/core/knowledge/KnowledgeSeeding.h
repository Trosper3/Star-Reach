#pragma once

#include "core/knowledge/KnowledgeNetwork.h"

// The KnowledgeStore seeder, mirroring core/diplomacy/RelationSeeding.h's role for
// DiplomacyMatrix: DiscoverySystem::Tick (features.md 8.3) grants a discovered system to
// `FactionNetworkId(faction)`, and KnowledgeStore::Grant is a no-op against an id Create() (or
// this) never registered -- a live ctx.knowledge pointer into a store with no faction networks in
// it is the same null-object failure architecture.md 12.32 already named for DiplomacyMatrix, one
// layer down.
namespace sr::core::knowledge {

// Registers one empty NetworkOwnerKind::Faction network per faction, keyed by FactionNetworkId --
// the same id StationFactory.cpp's placeholder NetworkOwner already assumes exists. Idempotent --
// safe to call once at startup, the same convention SeedBaselineRelations documents; calling it
// again would reset every faction's accumulated discoveries, so it must run before any system's
// first tick, never after.
void SeedFactionNetworks(KnowledgeStore& store);

}  // namespace sr::core::knowledge
