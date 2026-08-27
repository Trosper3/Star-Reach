#pragma once

#include <cstdint>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <span>
#include <string>
#include <vector>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/diplomacy/Territory.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "modes/space/render/WorldRenderer.h"
#include "shared/blueprints/Ids.h"

// modes/space/ui/NavigationMap -- architecture.md 12.6, features.md section 8.
//
// modes/*/ui/ must not include systems/ (section 2.3), so this reads core/knowledge/'s
// KnowledgeStore directly and the registry directly (the same access BridgeView already has) --
// never a systems/ header. Nothing here changes state, so there is no Intent to emit yet either:
// features.md section 8's RTS move order at zoom level 3 needs a galaxy-level in-transit-fleet
// concept that Law 2 itself flags as still unbuilt ("the awkward case... not yet built" --
// modes/space/data/SystemWorld.h). Adding a MoveFleetRequest with no consumer and no data model
// to consume it against would be a dead abstraction (section 2.4); that Intent lands with
// whichever issue builds fleet movement.
//
// Levels 1-2 render territory the player may never have visited (architecture.md 12.4's Seeding
// supplies unvisited systems), which is why they need no registry -- but Seeding only derives a
// seed FOR a given coordinate; nothing in this codebase yet enumerates every coordinate that holds
// an inhabited system, its display name, or its galaxy-map position (that roster is a separate,
// unbuilt piece of content infrastructure, not a NavigationMap concern, and the reason
// RegionalClusters below groups by owning faction rather than by real proximity). So levels 1-2
// are scoped here to what real data sources already answer: `core::diplomacy::Territory`'s claims
// (architecture.md 12.35) gated by the player's own faction's discovered systems
// (core/knowledge/KnowledgeNetwork.h's KnowledgeStore, keyed by core::knowledge::FactionNetworkId)
// -- not the full, roster-less galaxy. Level 3 is the resident system's own registry and needs no
// such roster.
namespace sr::space::ui::navigation_map {

enum class ZoomLevel : std::uint8_t {
    Galaxy = 1,
    Region = 2,
    System = 3,
    Tactical = 4,
};

// `faction`'s discovered system ids, sorted for deterministic iteration/tests. Empty if
// `faction`'s network is not registered in `knowledge` (fails closed, the same convention every
// other reader below uses). Pure -- no raylib -- so unit-testable. `RegionalClusters` below is
// levels 1-2's (Galaxy/Region) actual fog gate; this is the roster it gates against.
std::vector<std::string> DiscoveredSystemIds(const FactionId& faction,
                                             const core::knowledge::KnowledgeStore& knowledge);

// Whether a territory cluster's composition is visible to the viewing faction, or only its
// presence (features.md 8.3: "absence must never look like emptiness"). `TerritoryCluster::owner`
// is meaningless when this reads `Unknown` -- the empty `FactionId` there is a placeholder, not a
// claim that the systems are unclaimed.
enum class TerritoryKnowledge : std::uint8_t {
    Known,
    Unknown,
};

// One aggregate blob for the Level-1 sub-scales (architecture.md 12.35, features.md 8.1): either
// every system a faction owns that the viewer has discovered, or -- when `knowledge` is `Unknown`
// -- every claimed system the viewer has not. `systemIds` is sorted, for deterministic iteration
// and tests.
struct TerritoryCluster {
    FactionId owner;
    TerritoryKnowledge knowledge = TerritoryKnowledge::Known;
    std::vector<std::string> systemIds;
};

// Step one of "systems -> regional clusters -> galactic territory" (architecture.md 12.35): every
// system `territory` has ever claimed, plus every system `viewer`'s knowledge network has
// discovered (a discovered system may be unclaimed), grouped by owner -- the only grouping key
// this codebase has today. There is no galaxy-position/proximity data yet to cluster by (the gap
// NavigationMap.h's header comment already records for Level 1-2's layout), so "regional" here
// means "by owning faction," not "by location"; a real geometric partition is future work this
// return shape does not block. A system `viewer` has not discovered collapses into one
// `TerritoryKnowledge::Unknown` cluster regardless of its real owner (features.md 8.3) rather than
// being omitted. Clusters are ordered by owner id, Unknown last, for deterministic tests.
std::vector<TerritoryCluster> RegionalClusters(const FactionId& viewer,
                                               const core::diplomacy::Territory& territory,
                                               const core::knowledge::KnowledgeStore& knowledge);

// Step two: merges regional clusters that share an owner (and merges every `Unknown` cluster into
// one) into a single galaxy-wide picture per owner. With `RegionalClusters` grouping by owner
// alone (the comment above), this is the identity merge today -- once a real region partition
// exists and `RegionalClusters` can emit more than one cluster per owner, this is the step that
// recomposes them into the Galactic sub-scale's coarser picture.
std::vector<TerritoryCluster> GalacticTerritory(std::span<const TerritoryCluster> regionalClusters);

// features.md 8.2: ship/fleet icons are scoped to System (level 3) and culled entirely above it.
// This is correctness, not an optimization -- Galaxy/Region levels have no registry and
// therefore no entities to draw icons for in the first place.
bool ShowsShipIcons(ZoomLevel level);

// Sensor-gated fog of war for level 3 (architecture.md 12.6, "settled: sensor-coverage only"),
// now a real relation-band membership test rather than faction inequality (architecture.md 15.1
// finding 17, the same simplification finding N records for TargetingSystem): every Targetable
// rig root within `player`'s own SensorRange whose stance toward `player`'s faction reads Hostile
// or War on `diplomacy` -- features.md 5.3's "fired on" bands, Neutral/Friendly/Allied rigs never
// appear. Gated a second way on `systemId` having been discovered by `player`'s faction's network
// on `knowledge` -- finding 17's other half. `player` must carry FactionRef, WorldTransform, and
// SensorRange, and both `diplomacy` and `knowledge` must be non-null, or the result is empty
// (fails closed, the same convention TargetingSystem/DockingSystem use). Pure -- no raylib -- so
// unit-testable without a live GL context.
std::vector<entt::entity> VisibleHostileRigs(const entt::registry& registry, entt::entity player,
                                             const core::diplomacy::DiplomacyMatrix* diplomacy,
                                             const core::knowledge::KnowledgeStore* knowledge,
                                             const std::string& systemId);

// Draws the map at `level` for `playerFaction` in `systemId`. Galaxy/Region render
// `RegionalClusters`/`GalacticTerritory`'s territory-aggregate blobs, not individual system
// markers (architecture.md 12.35, features.md 8.1 -- "every scale shows territory, never
// individual objects"), colored by `playerFaction`'s relation to each cluster's owner and shaped
// by its `TerritoryKnowledge` (render::MapMarkerKind::Territory vs Unknown); System projects
// VisibleHostileRigs through `camera` the same way DrawTargetReticle does (render/IconRenderer.h)
// and draws a render::MapMarkerKind::Hostile marker; Tactical is a no-op here -- level 4 is the
// existing WorldRenderer path (architecture.md 12.6), not a second renderer.
void Draw(const entt::registry& registry, ZoomLevel level, const FactionId& playerFaction,
          const std::string& systemId, const core::knowledge::KnowledgeStore& knowledge,
          const core::diplomacy::DiplomacyMatrix& diplomacy,
          const core::diplomacy::Territory& territory, const render::CameraView& camera);

}  // namespace sr::space::ui::navigation_map
