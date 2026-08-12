#include "modes/space/systems/LootSystem.h"

#include <entt/entity/entity.hpp>
#include <vector>

#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Physics.h"
#include "shared/components/Transform.h"

namespace sr::space::loot_system {
namespace {

// A wreck's WorldBody radius is purely a draw size -- pickup range for a DeathWreck derives from
// the collector, the same as MaterialDrop (MiningSystem.cpp); only DerelictWreck's own
// radiusUnits widens the pickup check itself.
constexpr float kDeathWreckRadius = 20.0f;

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

void TickDeathWrecks(entt::registry& registry, float dt) {
    std::vector<entt::entity> toDestroy;
    for (auto [wreck, deathWreck, xf] : registry.view<DeathWreck, WorldTransform>().each()) {
        entt::entity collector = entt::null;
        if (FindCollectorInRange(registry, xf.position, 0.0f, collector)) {
            for (const ModuleId& moduleId : deathWreck.modules) {
                AddModule(registry, collector, moduleId);
            }
            for (const MaterialStack& stack : deathWreck.materials) {
                AddMaterial(registry, collector, stack.materialId, stack.quantity);
            }
            toDestroy.push_back(wreck);
            continue;
        }
        deathWreck.lifetimeSeconds -= dt;
        if (deathWreck.lifetimeSeconds <= 0.0f) {
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
    TickDeathWrecks(registry, ctx.dt);
}

core::galaxy::WreckRecord CollapseDeathWreck(entt::registry& registry, entt::entity entity,
                                             const std::string& systemId) {
    const DeathWreck& wreck = registry.get<DeathWreck>(entity);
    const WorldTransform& xf = registry.get<WorldTransform>(entity);

    core::galaxy::WreckRecord record;
    record.systemId = systemId;
    record.position = xf.position;
    record.modules = wreck.modules;
    record.materials = wreck.materials;
    record.lifetimeSeconds = wreck.lifetimeSeconds;

    registry.destroy(entity);
    return record;
}

entt::entity PromoteDeathWreck(entt::registry& registry, const core::galaxy::WreckRecord& record) {
    const entt::entity entity = registry.create();
    registry.emplace<WorldTransform>(entity, record.position, 0.0f);
    registry.emplace<WorldBody>(entity, kDeathWreckRadius, BodyKind::Wreck);
    registry.emplace<DeathWreck>(entity, record.modules, record.materials, record.lifetimeSeconds);
    return entity;
}

}  // namespace sr::space::loot_system
