#include "modes/space/systems/MiningSystem.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "shared/components/Loot.h"
#include "shared/components/Mining.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"

namespace sr::space::mining_system {
namespace {

// A pure function of (entity, material index, tick) rather than global RNG state -- Law 2's
// coarse-tick fast-forward needs every time-dependent decision to be reproducible from the same
// inputs, the same reason WeaponSystem's pellet fan avoids true randomness for spread.
std::uint32_t Hash32(std::uint32_t seed) {
    std::uint32_t hash = 2166136261u;
    for (int byte = 0; byte < 4; ++byte) {
        hash ^= (seed >> (byte * 8)) & 0xFFu;
        hash *= 16777619u;
    }
    return hash;
}

bool RollPercent(entt::entity entity, std::size_t materialIndex, unsigned long long tick,
                 int percent) {
    const auto seed = static_cast<std::uint32_t>(entt::to_integral(entity)) * 2654435761u +
                      static_cast<std::uint32_t>(materialIndex) * 40503u +
                      static_cast<std::uint32_t>(tick);
    return static_cast<int>(Hash32(seed) % 100u) < percent;
}

void SpawnMaterialDrop(entt::registry& registry, const Vec2& position,
                       const std::string& materialId) {
    const entt::entity drop = registry.create();
    registry.emplace<WorldTransform>(drop, position, 0.0f);
    registry.emplace<MaterialDrop>(drop, materialId, 1, 28.0f);
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> depleted;

    for (auto [entity, composition, xf] :
         registry.view<Asteroid, AsteroidComposition, WorldTransform, Destroyed>().each()) {
        for (std::size_t i = 0; i < composition.materials.size(); ++i) {
            const MaterialChance& chance = composition.materials[i];
            if (RollPercent(entity, i, ctx.tick, chance.percent)) {
                SpawnMaterialDrop(registry, xf.position, chance.materialId);
            }
        }
        depleted.push_back(entity);
    }

    for (const entt::entity entity : depleted) {
        registry.destroy(entity);
    }
}

}  // namespace sr::space::mining_system
