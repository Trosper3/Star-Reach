#pragma once

#include <cstdint>
#include <entt/entity/entity.hpp>
#include <entt/entity/registry.hpp>
#include <string>
#include <vector>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/galaxy/Discovery.h"
#include "modes/space/render/WorldRenderer.h"
#include "shared/blueprints/Ids.h"

// modes/space/ui/NavigationMap -- architecture.md 12.6, features.md section 8.
//
// modes/*/ui/ must not include systems/ (section 2.3), so this reads core/galaxy/'s
// DiscoveryState directly and the registry directly (the same access BridgeView already has) --
// never a systems/ header. Nothing here changes state, so there is no Intent to emit yet either:
// features.md section 8's RTS move order at zoom level 3 needs a galaxy-level in-transit-fleet
// concept that Law 2 itself flags as still unbuilt ("the awkward case... not yet built" --
// modes/space/data/SystemWorld.h). Adding a MoveFleetRequest with no consumer and no data model
// to consume it against would be a dead abstraction (section 2.4); that Intent lands with
// whichever issue builds fleet movement.
//
// Levels 1-2 render systems the player has never visited (architecture.md 12.4's Seeding
// supplies those), which is why they need no registry -- but Seeding only derives a seed FOR a
// given coordinate; nothing in this codebase yet enumerates which coordinates hold inhabited
// systems, their display names, or their galaxy-map positions (that roster is a separate,
// unbuilt piece of content infrastructure, not a NavigationMap concern). So levels 1-2 are
// scoped here to what a real data source already answers: the player's own faction's discovered
// systems (core/galaxy/Discovery.h), not the full galaxy. Level 3 is the resident system's own
// registry and needs no such roster.
namespace sr::space::ui::navigation_map {

enum class ZoomLevel : std::uint8_t {
    Galaxy = 1,
    Region = 2,
    System = 3,
    Tactical = 4,
};

// `faction`'s discovered system ids, sorted for deterministic iteration/tests. Pure -- no
// raylib -- so unit-testable. What levels 1-2 (Galaxy/Region) draw.
std::vector<std::string> DiscoveredSystemIds(const FactionId& faction,
                                             const core::galaxy::DiscoveryState& discovery);

// features.md 8.2: ship/fleet icons are scoped to System (level 3) and culled entirely above it.
// This is correctness, not an optimization -- Galaxy/Region levels have no registry and
// therefore no entities to draw icons for in the first place.
bool ShowsShipIcons(ZoomLevel level);

// Sensor-gated fog of war for level 3 (architecture.md 12.6, "settled: sensor-coverage only"),
// now a real relation-band membership test rather than faction inequality (architecture.md 15.1
// finding 17, the same simplification finding N records for TargetingSystem): every Targetable
// rig root within `player`'s own SensorRange whose stance toward `player`'s faction reads Hostile
// or War on `diplomacy` -- features.md 5.3's "fired on" bands, Neutral/Friendly/Allied rigs never
// appear. Gated a second way on `systemId` having been discovered by `player`'s faction on
// `discovery` -- finding 17's other half. `player` must carry FactionRef, WorldTransform, and
// SensorRange, and both `diplomacy` and `discovery` must be non-null, or the result is empty
// (fails closed, the same convention TargetingSystem/DockingSystem use). Pure -- no raylib -- so
// unit-testable without a live GL context.
std::vector<entt::entity> VisibleHostileRigs(const entt::registry& registry, entt::entity player,
                                             const core::diplomacy::DiplomacyMatrix* diplomacy,
                                             const core::galaxy::DiscoveryState* discovery,
                                             const std::string& systemId);

// Draws the map at `level` for `playerFaction` in `systemId`. Galaxy/Region draw
// DiscoveredSystemIds as fixed screen-space markers with a placeholder layout (this header's
// comment); System projects VisibleHostileRigs through `camera` the same way DrawTargetReticle
// does (render/IconRenderer.h) and draws the same fixed-size marker; Tactical is a no-op here --
// level 4 is the existing WorldRenderer path (architecture.md 12.6), not a second renderer.
void Draw(const entt::registry& registry, ZoomLevel level, const FactionId& playerFaction,
          const std::string& systemId, const core::galaxy::DiscoveryState& discovery,
          const core::diplomacy::DiplomacyMatrix& diplomacy, const render::CameraView& camera);

}  // namespace sr::space::ui::navigation_map
