#include "modes/space/systems/LootSystem.h"

#include <entt/entity/entity.hpp>
#include <vector>

#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Physics.h"
#include "shared/components/Transform.h"

namespace sr::space::loot_system {
namespace {

void AddModule(entt::registry& registry, entt::entity collector, const ModuleId& moduleId) {
    registry.get_or_emplace<CargoHold>(collector).modules.push_back(moduleId);
}

void AddMaterial(entt::registry& registry, entt::entity collector, const std::string& materialId,
                 int quantity) {
    CargoHold& cargo = registry.get_or_emplace<CargoHold>(collector);
    for (MaterialStack& stack : cargo.materials) {
        if (stack.materialId == materialId) {
            stack.quantity += quantity;
            return;
        }
    }
    cargo.materials.push_back(MaterialStack{materialId, quantity});
}

void AddCredits(entt::registry& registry, entt::entity collector, int credits) {
    registry.get_or_emplace<Wallet>(collector).credits += credits;
}

// True if some PlayerControlled collector's own CollisionRadius (widened by `extraRadius`, for
// drops physically bigger than a point) reaches `position`. Pickup range is deliberately never
// a constant of its own -- it derives entirely from the collector, matching the ship-scaling
// work this issue calls out.
bool FindCollectorInRange(entt::registry& registry, const Vec2& position, float extraRadius,
                          entt::entity& outCollector) {
    for (auto [collector, xf, radius] :
         registry.view<PlayerControlled, WorldTransform, CollisionRadius>().each()) {
        if (Distance(xf.position, position) <= radius.value + extraRadius) {
            outCollector = collector;
            return true;
        }
    }
    return false;
}

void TickLootDrops(entt::registry& registry, float dt) {
    std::vector<entt::entity> toDestroy;
    for (auto [drop, loot, xf] : registry.view<LootDrop, WorldTransform>().each()) {
        entt::entity collector = entt::null;
        if (FindCollectorInRange(registry, xf.position, 0.0f, collector)) {
            AddModule(registry, collector, loot.moduleId);
            toDestroy.push_back(drop);
            continue;
        }
        loot.lifetimeSeconds -= dt;
        if (loot.lifetimeSeconds <= 0.0f) {
            toDestroy.push_back(drop);
        }
    }
    for (const entt::entity drop : toDestroy) {
        registry.destroy(drop);
    }
}

void TickMaterialDrops(entt::registry& registry, float dt) {
    std::vector<entt::entity> toDestroy;
    for (auto [drop, material, xf] : registry.view<MaterialDrop, WorldTransform>().each()) {
        entt::entity collector = entt::null;
        if (FindCollectorInRange(registry, xf.position, 0.0f, collector)) {
            AddMaterial(registry, collector, material.materialId, material.quantity);
            toDestroy.push_back(drop);
            continue;
        }
        material.lifetimeSeconds -= dt;
        if (material.lifetimeSeconds <= 0.0f) {
            toDestroy.push_back(drop);
        }
    }
    for (const entt::entity drop : toDestroy) {
        registry.destroy(drop);
    }
}

// No lifetime countdown -- a DerelictWreck never ages out (LootSystem.h).
void TickDerelictWrecks(entt::registry& registry) {
    std::vector<entt::entity> toDestroy;
    for (auto [wreck, derelict, xf] : registry.view<DerelictWreck, WorldTransform>().each()) {
        entt::entity collector = entt::null;
        if (FindCollectorInRange(registry, xf.position, derelict.radiusUnits, collector)) {
            AddCredits(registry, collector, derelict.creditsReward);
            toDestroy.push_back(wreck);
        }
    }
    for (const entt::entity wreck : toDestroy) {
        registry.destroy(wreck);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    TickLootDrops(registry, ctx.dt);
    TickMaterialDrops(registry, ctx.dt);
    TickDerelictWrecks(registry);
}

}  // namespace sr::space::loot_system
