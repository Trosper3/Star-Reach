#include "modes/space/ui/NavigationMap.h"

#include <raylib.h>
#include <algorithm>
#include <cmath>

#include "modes/space/render/IconRenderer.h"
#include "shared/components/Identity.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"
#include "shared/ui/HudTheme.h"

namespace sr::space::ui::navigation_map {
namespace {

entt::entity FindPlayer(const entt::registry& registry) {
    for (auto [entity] : registry.view<PlayerControlled>().each()) {
        return entity;
    }
    return entt::null;
}

// Arranges `count` markers evenly around a screen-centered circle -- levels 1-2 have no galaxy
// layout to lay out against (this header's comment), so this is a placeholder arrangement, not a
// projection of real coordinates.
Vec2 CircleLayoutPosition(std::size_t index, std::size_t count, float radius) {
    const float angle =
        count == 0 ? 0.0f : (static_cast<float>(index) / static_cast<float>(count)) * 6.2831853f;
    return Vec2{GetScreenWidth() * 0.5f + radius * std::cos(angle),
                GetScreenHeight() * 0.5f + radius * std::sin(angle)};
}

}  // namespace

std::vector<std::string> DiscoveredSystemIds(const FactionId& faction,
                                             const core::galaxy::DiscoveryState& discovery) {
    std::vector<std::string> ids(discovery.Discovered(faction).begin(),
                                 discovery.Discovered(faction).end());
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool ShowsShipIcons(ZoomLevel level) {
    return level == ZoomLevel::System;
}

std::vector<entt::entity> VisibleHostileRigs(const entt::registry& registry, entt::entity player,
                                             const core::diplomacy::DiplomacyMatrix* diplomacy,
                                             const core::galaxy::DiscoveryState* discovery,
                                             const std::string& systemId) {
    std::vector<entt::entity> visible;
    if (diplomacy == nullptr || discovery == nullptr) {
        return visible;
    }
    if (!registry.all_of<FactionRef, WorldTransform, SensorRange>(player)) {
        return visible;
    }

    const FactionId& playerFaction = registry.get<FactionRef>(player).id;
    if (!discovery->IsDiscovered(playerFaction, systemId)) {
        return visible;
    }
    const Vec2 playerPosition = registry.get<WorldTransform>(player).position;
    const float sensorRangeSq = [&] {
        const float range = registry.get<SensorRange>(player).units;
        return range * range;
    }();

    for (auto [entity, faction, xf] :
         registry.view<FactionRef, WorldTransform, Targetable>().each()) {
        if (diplomacy->Get(playerFaction, faction.id) > core::diplomacy::Relation::Hostile) {
            continue;
        }
        if (DistanceSquared(playerPosition, xf.position) <= sensorRangeSq) {
            visible.push_back(entity);
        }
    }
    return visible;
}

void Draw(const entt::registry& registry, ZoomLevel level, const FactionId& playerFaction,
          const std::string& systemId, const core::galaxy::DiscoveryState& discovery,
          const core::diplomacy::DiplomacyMatrix& diplomacy, const render::CameraView& camera) {
    if (level == ZoomLevel::Galaxy || level == ZoomLevel::Region) {
        const std::vector<std::string> systemIds = DiscoveredSystemIds(playerFaction, discovery);
        const float radius = level == ZoomLevel::Galaxy ? 260.0f : 140.0f;
        for (std::size_t i = 0; i < systemIds.size(); ++i) {
            const Vec2 screenPos = CircleLayoutPosition(i, systemIds.size(), radius);
            render::DrawMapMarker(screenPos, sr::ui::kStatusGood, systemIds[i]);
        }
        return;
    }

    if (level == ZoomLevel::System) {
        const entt::entity player = FindPlayer(registry);
        if (player == entt::null) {
            return;
        }
        for (const entt::entity hostile :
             VisibleHostileRigs(registry, player, &diplomacy, &discovery, systemId)) {
            const Vec2 worldPos = registry.get<WorldTransform>(hostile).position;
            render::DrawMapMarker(render::WorldToScreen(worldPos, camera), sr::ui::kStatusCritical,
                                  "");
        }
    }

    // Tactical (level 4) is the existing WorldRenderer path -- nothing to draw here.
}

}  // namespace sr::space::ui::navigation_map
