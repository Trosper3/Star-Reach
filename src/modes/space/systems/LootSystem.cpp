#include "modes/space/systems/LootSystem.h"

#include <entt/entity/entity.hpp>
#include <vector>

#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/rig/CargoView.h"

namespace sr::space::loot_system {
namespace {

// A wreck's WorldBody radius is purely a draw size -- pickup range for a DeathWreck derives from
// the collector, the same as MaterialDrop (MiningSystem.cpp); only DerelictWreck's own
// radiusUnits widens the pickup check itself.
constexpr float kDeathWreckRadius = 20.0f;

// Matches MiningSystem.cpp's kMaterialDropRadius -- a spilled stack is the same kind of loose
// battlefield object a mined asteroid produces, just from a different source.
constexpr float kSpilledDropRadius = 6.0f;

// Deposits one unit of `moduleId` into the collector's cargo bays. False (and nothing written)
// if no bay has room -- the caller leaves the drop in place rather than discarding it.
bool AddModule(entt::registry& registry, entt::entity collector, const ModuleId& moduleId,
               const core::ContentLibrary& content) {
    const ModuleDef* module = content.FindModule(moduleId);
    const float mass = module != nullptr ? module->mass : 0.0f;
    const auto result = cargo_view::Deposit(registry, collector,
                                            ItemStack{ItemKind::Module, moduleId.str(), 1, mass});
    return result == cargo_view::DepositResult::Deposited;
}

// Deposits `quantity` of `materialId` into the collector's cargo bays, topping up an existing
// stack first. False (and nothing written) if the full quantity has no room anywhere.
bool AddMaterial(entt::registry& registry, entt::entity collector, const std::string& materialId,
                 int quantity, const core::ContentLibrary& content) {
    const MaterialDef* material = content.FindMaterial(MaterialId(materialId));
    const float mass = material != nullptr ? material->mass : 0.0f;
    const auto result = cargo_view::Deposit(
        registry, collector, ItemStack{ItemKind::Material, materialId, quantity, mass});
    return result == cargo_view::DepositResult::Deposited;
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

void TickLootDrops(entt::registry& registry, const core::ContentLibrary& content, float dt) {
    std::vector<entt::entity> toDestroy;
    for (auto [drop, loot, xf] : registry.view<LootDrop, WorldTransform>().each()) {
        entt::entity collector = entt::null;
        if (FindCollectorInRange(registry, xf.position, 0.0f, collector) &&
            AddModule(registry, collector, loot.moduleId, content)) {
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

void TickMaterialDrops(entt::registry& registry, const core::ContentLibrary& content, float dt) {
    std::vector<entt::entity> toDestroy;
    for (auto [drop, material, xf] : registry.view<MaterialDrop, WorldTransform>().each()) {
        entt::entity collector = entt::null;
        if (FindCollectorInRange(registry, xf.position, 0.0f, collector) &&
            AddMaterial(registry, collector, material.materialId, material.quantity, content)) {
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

void TickDeathWrecks(entt::registry& registry, const core::ContentLibrary& content, float dt) {
    std::vector<entt::entity> toDestroy;
    for (auto [wreck, deathWreck, xf] : registry.view<DeathWreck, WorldTransform>().each()) {
        entt::entity collector = entt::null;
        if (FindCollectorInRange(registry, xf.position, 0.0f, collector)) {
            // Best-effort: a wreck's manifest is a one-shot snapshot, not a live hold, so an item
            // that finds no room is simply lost rather than requeued -- the wreck is destroyed
            // unconditionally below either way, the same as before P0-10.
            for (const ModuleId& moduleId : deathWreck.modules) {
                AddModule(registry, collector, moduleId, content);
            }
            for (const MaterialStack& stack : deathWreck.materials) {
                AddMaterial(registry, collector, stack.materialId, stack.quantity, content);
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

// The producer half: every cargo-bay hardpoint tagged Destroyed this tick spills exactly its own
// stacks. DamageSystem only tags Destroyed and never destroys the hardpoint entity, so the
// component is still here to read on the following tick -- no new lifetime rule needed.
void SpillDestroyedBays(entt::registry& registry) {
    for (const entt::entity hardpoint : registry.view<CargoHold, Destroyed>()) {
        SpillCargoHold(registry, hardpoint);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    TickLootDrops(registry, ctx.content, ctx.dt);
    TickMaterialDrops(registry, ctx.content, ctx.dt);
    TickDerelictWrecks(registry);
    TickDeathWrecks(registry, ctx.content, ctx.dt);
    SpillDestroyedBays(registry);
}

void SpillCargoHold(entt::registry& registry, entt::entity hardpoint) {
    CargoHold* cargo = registry.try_get<CargoHold>(hardpoint);
    if (cargo == nullptr || cargo->stacks.empty()) {
        return;
    }
    const auto* xf = registry.try_get<WorldTransform>(hardpoint);
    const Vec2 position = xf != nullptr ? xf->position : Vec2{};

    for (const ItemStack& stack : cargo->stacks) {
        const entt::entity drop = registry.create();
        registry.emplace<WorldTransform>(drop, position, 0.0f);
        registry.emplace<WorldBody>(drop, kSpilledDropRadius, BodyKind::Drop);
        if (stack.kind == ItemKind::Module) {
            registry.emplace<LootDrop>(drop, ModuleId(stack.id));
        } else {
            registry.emplace<MaterialDrop>(drop, stack.id, stack.quantity);
        }
    }
    cargo->stacks.clear();
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
