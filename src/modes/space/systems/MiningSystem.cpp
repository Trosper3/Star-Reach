#include "modes/space/systems/MiningSystem.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include "shared/components/Loot.h"
#include "shared/components/Mining.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"

namespace sr::space::mining_system {
namespace {

// A drop's WorldBody radius is purely a draw/pickup-visibility size, not a gameplay collision
// radius -- FindCollectorInRange (LootSystem.cpp) derives pickup range from the collector, not
// the drop.
constexpr float kElementDropRadius = 6.0f;

// A pure function of (entity, element index, tick) rather than global RNG state -- Law 2's
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

bool RollPercent(entt::entity entity, std::size_t elementIndex, unsigned long long tick,
                 int percent) {
    const auto seed = static_cast<std::uint32_t>(entt::to_integral(entity)) * 2654435761u +
                      static_cast<std::uint32_t>(elementIndex) * 40503u +
                      static_cast<std::uint32_t>(tick);
    return static_cast<int>(Hash32(seed) % 100u) < percent;
}

void SpawnElementDrop(entt::registry& registry, const Vec2& position,
                      const std::string& elementId) {
    const entt::entity drop = registry.create();
    registry.emplace<WorldTransform>(drop, position, 0.0f);
    registry.emplace<WorldBody>(drop, kElementDropRadius, BodyKind::Drop);
    registry.emplace<ElementDrop>(drop, elementId, 1, 28.0f);
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> depleted;

    for (auto [entity, composition, xf] :
         registry.view<Asteroid, AsteroidComposition, WorldTransform, Destroyed>().each()) {
        for (std::size_t i = 0; i < composition.elements.size(); ++i) {
            const ElementChance& chance = composition.elements[i];
            if (RollPercent(entity, i, ctx.tick, chance.percent)) {
                SpawnElementDrop(registry, xf.position, chance.elementId);
            }
        }
        depleted.push_back(entity);
    }

    for (const entt::entity entity : depleted) {
        registry.destroy(entity);
    }
}

}  // namespace sr::space::mining_system
