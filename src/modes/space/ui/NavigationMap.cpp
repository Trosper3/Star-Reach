#include "modes/space/ui/NavigationMap.h"

#include <raylib.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

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

// Shared by RegionalClusters and GalacticTerritory: turns a per-owner bucket map plus a pooled
// "unknown" bucket into the sorted TerritoryCluster list both functions return -- owners in
// ascending FactionId order, Unknown (if non-empty) last, every systemIds list sorted.
std::vector<TerritoryCluster> ClustersFromBuckets(
    std::map<FactionId, std::vector<std::string>> byOwner, std::vector<std::string> unknown) {
    std::vector<TerritoryCluster> clusters;
    clusters.reserve(byOwner.size() + 1);
    for (auto& [owner, ids] : byOwner) {
        std::sort(ids.begin(), ids.end());
        clusters.push_back(TerritoryCluster{owner, TerritoryKnowledge::Known, std::move(ids)});
    }
    if (!unknown.empty()) {
        std::sort(unknown.begin(), unknown.end());
        clusters.push_back(
            TerritoryCluster{FactionId(), TerritoryKnowledge::Unknown, std::move(unknown)});
    }
    return clusters;
}

// A cluster's presence/relation color: dim for anything the viewer can't attribute (fog, or known
// but unclaimed), good for the viewer's own faction, critical for a Hostile/War relation, and
// bright-neutral for any other known foreign owner (features.md 5.3's bands, the same threshold
// VisibleHostileRigs uses for its Hostile/War cutoff).
Color ColorForCluster(const TerritoryCluster& cluster, const FactionId& viewer,
                      const core::diplomacy::DiplomacyMatrix& diplomacy) {
    if (cluster.knowledge == TerritoryKnowledge::Unknown || cluster.owner.empty()) {
        return sr::ui::kLabelDim;
    }
    if (cluster.owner == viewer) {
        return sr::ui::kStatusGood;
    }
    if (diplomacy.Get(viewer, cluster.owner) <= core::diplomacy::Relation::Hostile) {
        return sr::ui::kStatusCritical;
    }
    return sr::ui::kValueBright;
}

// Unknown clusters reveal nothing beyond their presence (features.md 8.3); known clusters label
// with the owner (or "unclaimed") and how many systems the blob aggregates.
std::string LabelForCluster(const TerritoryCluster& cluster) {
    if (cluster.knowledge == TerritoryKnowledge::Unknown) {
        return "";
    }
    const std::string ownerLabel = cluster.owner.empty() ? "unclaimed" : cluster.owner.str();
    return ownerLabel + " (" + std::to_string(cluster.systemIds.size()) + ")";
}

}  // namespace

std::vector<std::string> DiscoveredSystemIds(const FactionId& faction,
                                             const core::knowledge::KnowledgeStore& knowledge) {
    const core::knowledge::KnowledgeNetwork* network =
        knowledge.Get(core::knowledge::FactionNetworkId(faction));
    if (network == nullptr) {
        return {};
    }
    std::vector<std::string> ids(network->discoveredSystems.begin(),
                                 network->discoveredSystems.end());
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<TerritoryCluster> RegionalClusters(const FactionId& viewer,
                                               const core::diplomacy::Territory& territory,
                                               const core::knowledge::KnowledgeStore& knowledge) {
    const std::vector<std::string> discovered = DiscoveredSystemIds(viewer, knowledge);
    const std::set<std::string> discoveredSet(discovered.begin(), discovered.end());

    std::set<std::string> allIds(discovered.begin(), discovered.end());
    for (const auto& claim : territory.ClaimedSystems()) {
        allIds.insert(claim.first);
    }

    std::map<FactionId, std::vector<std::string>> byOwner;
    std::vector<std::string> unknown;
    for (const std::string& id : allIds) {
        if (discoveredSet.contains(id)) {
            byOwner[territory.Owner(id)].push_back(id);
        } else {
            unknown.push_back(id);
        }
    }

    return ClustersFromBuckets(std::move(byOwner), std::move(unknown));
}

std::vector<TerritoryCluster> GalacticTerritory(
    std::span<const TerritoryCluster> regionalClusters) {
    std::map<FactionId, std::vector<std::string>> byOwner;
    std::vector<std::string> unknown;
    for (const TerritoryCluster& cluster : regionalClusters) {
        if (cluster.knowledge == TerritoryKnowledge::Unknown) {
            unknown.insert(unknown.end(), cluster.systemIds.begin(), cluster.systemIds.end());
        } else {
            std::vector<std::string>& bucket = byOwner[cluster.owner];
            bucket.insert(bucket.end(), cluster.systemIds.begin(), cluster.systemIds.end());
        }
    }
    return ClustersFromBuckets(std::move(byOwner), std::move(unknown));
}

bool ShowsShipIcons(ZoomLevel level) {
    return level == ZoomLevel::System;
}

std::vector<entt::entity> VisibleHostileRigs(const entt::registry& registry, entt::entity player,
                                             const core::diplomacy::DiplomacyMatrix* diplomacy,
                                             const core::knowledge::KnowledgeStore* knowledge,
                                             const std::string& systemId) {
    std::vector<entt::entity> visible;
    if (diplomacy == nullptr || knowledge == nullptr) {
        return visible;
    }
    if (!registry.all_of<FactionRef, WorldTransform, SensorRange>(player)) {
        return visible;
    }

    const FactionId& playerFaction = registry.get<FactionRef>(player).id;
    const core::knowledge::KnowledgeNetwork* network =
        knowledge->Get(core::knowledge::FactionNetworkId(playerFaction));
    if (network == nullptr || !network->discoveredSystems.contains(systemId)) {
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
          const std::string& systemId, const core::knowledge::KnowledgeStore& knowledge,
          const core::diplomacy::DiplomacyMatrix& diplomacy,
          const core::diplomacy::Territory& territory, const render::CameraView& camera) {
    if (level == ZoomLevel::Galaxy || level == ZoomLevel::Region) {
        const std::vector<TerritoryCluster> regional =
            RegionalClusters(playerFaction, territory, knowledge);
        const std::vector<TerritoryCluster> clusters =
            level == ZoomLevel::Region ? regional : GalacticTerritory(regional);
        const float radius = level == ZoomLevel::Galaxy ? 260.0f : 140.0f;
        for (std::size_t i = 0; i < clusters.size(); ++i) {
            const TerritoryCluster& cluster = clusters[i];
            const Vec2 screenPos = CircleLayoutPosition(i, clusters.size(), radius);
            const render::MapMarkerKind kind = cluster.knowledge == TerritoryKnowledge::Unknown
                                                   ? render::MapMarkerKind::Unknown
                                                   : render::MapMarkerKind::Territory;
            render::DrawMapMarker(screenPos, ColorForCluster(cluster, playerFaction, diplomacy),
                                  LabelForCluster(cluster), kind, camera.zoom);
        }
        return;
    }

    if (level == ZoomLevel::System) {
        const entt::entity player = FindPlayer(registry);
        if (player == entt::null) {
            return;
        }
        for (const entt::entity hostile :
             VisibleHostileRigs(registry, player, &diplomacy, &knowledge, systemId)) {
            const Vec2 worldPos = registry.get<WorldTransform>(hostile).position;
            render::DrawMapMarker(render::WorldToScreen(worldPos, camera), sr::ui::kStatusCritical,
                                  "", render::MapMarkerKind::Hostile, camera.zoom);
        }
    }

    // Tactical (level 4) is the existing WorldRenderer path -- nothing to draw here.
}

}  // namespace sr::space::ui::navigation_map
