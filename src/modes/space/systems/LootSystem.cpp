#include "modes/space/systems/LootSystem.h"

#include <entt/entity/entity.hpp>
#include <vector>

#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Spawn.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/rig/CargoView.h"
#include "shared/rig/ModuleAttachment.h"

namespace sr::space::loot_system {
namespace {

// A wreck's WorldBody radius is purely a draw size -- pickup range for a DeathWreck derives from
// the collector, the same as ElementDrop (MiningSystem.cpp); only DerelictWreck's own
// radiusUnits widens the pickup check itself.
constexpr float kDeathWreckRadius = 20.0f;

// Matches MiningSystem.cpp's kElementDropRadius -- a spilled stack is the same kind of loose
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

// Deposits `quantity` of `elementId` into the collector's cargo bays, topping up an existing
// stack first. False (and nothing written) if the full quantity has no room anywhere.
bool AddElement(entt::registry& registry, entt::entity collector, const std::string& elementId,
                int quantity, const core::ContentLibrary& content) {
    const ElementDef* element = content.FindElement(ElementId(elementId));
    const float mass = element != nullptr ? element->mass : 0.0f;
    const auto result = cargo_view::Deposit(
        registry, collector, ItemStack{ItemKind::Element, elementId, quantity, mass});
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

void TickElementDrops(entt::registry& registry, const core::ContentLibrary& content, float dt) {
    std::vector<entt::entity> toDestroy;
    for (auto [drop, element, xf] : registry.view<ElementDrop, WorldTransform>().each()) {
        entt::entity collector = entt::null;
        if (FindCollectorInRange(registry, xf.position, 0.0f, collector) &&
            AddElement(registry, collector, element.elementId, element.quantity, content)) {
            toDestroy.push_back(drop);
            continue;
        }
        element.lifetimeSeconds -= dt;
        if (element.lifetimeSeconds <= 0.0f) {
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
            for (const ElementStack& stack : deathWreck.elements) {
                AddElement(registry, collector, stack.elementId, stack.quantity, content);
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

// Tops up an existing matching entry rather than appending a duplicate -- ElementStack's
// documented merge-by-id contract (Loot.h), the same rule a collector's own pickup honors.
void AddToManifest(std::vector<ElementStack>& elements, const std::string& elementId,
                   int quantity) {
    for (ElementStack& stack : elements) {
        if (stack.elementId == elementId) {
            stack.quantity += quantity;
            return;
        }
    }
    elements.push_back(ElementStack{elementId, quantity});
}

// features.md section 3.3 Tier 1/2, architecture.md 13.3 R: a PlayerControlled rig with every
// hardpoint gone is the player dying. The vessel and everything mounted or held on it is
// permanently lost -- collected into one DeathWreck at the death position (the same dual-form
// promotion CollapseDeathWreck/PromoteDeathWreck already round-trip across a warp) rather than
// discarded outright, per the recovery run's "cargo and equipped modules drop as a recoverable
// wreck" rule.
//
// There is no live-rig-to-blueprint snapshot capability in this codebase yet (P12.31's RigState,
// SpaceFlight.h's WarpToSystem doc comment) and modes/space/systems/ may not include factories/
// (Law 5) to build a replacement hull, so the same hardpoints are stripped bare and revived in
// place instead of being destroyed and rebuilt -- the closest reachable approximation of
// features.md's Starter Ship safety net available from inside a system. RespawnPending is only
// handed to SpawnSystem (architecture.md 13.3 R) to carry the bare hull home; it never touches
// Health or Destroyed itself (modes/space/systems/SpawnSystem.cpp), which is why the hull is
// revived here.
void HandlePlayerDeath(entt::registry& registry, const core::ContentLibrary& content) {
    std::vector<entt::entity> dead;
    for (const entt::entity root : registry.view<PlayerControlled, Rig, Destroyed>()) {
        dead.push_back(root);
    }

    for (const entt::entity root : dead) {
        const Vec2 position = registry.get<WorldTransform>(root).position;
        const std::vector<entt::entity> hardpoints = registry.get<Rig>(root).children;

        std::vector<ModuleId> modules;
        std::vector<ElementStack> elements;
        for (const entt::entity hardpoint : hardpoints) {
            if (auto* mounted = registry.try_get<MountedModules>(hardpoint)) {
                for (const ModuleId& moduleId : mounted->ids) {
                    modules.push_back(moduleId);
                    if (const ModuleDef* module = content.FindModule(moduleId)) {
                        rig_attachment::DetachModuleComponents(registry, hardpoint, *module);
                    }
                }
                mounted->ids.clear();
            }
            if (auto* cargo = registry.try_get<CargoHold>(hardpoint)) {
                for (const ItemStack& stack : cargo->stacks) {
                    if (stack.kind == ItemKind::Module) {
                        modules.push_back(ModuleId(stack.id));
                    } else {
                        AddToManifest(elements, stack.id, stack.quantity);
                    }
                }
                cargo->stacks.clear();
            }
            if (auto* health = registry.try_get<Health>(hardpoint)) {
                health->current = health->max;
            }
            registry.remove<Destroyed>(hardpoint);
        }

        registry.remove<Destroyed>(root);
        registry.emplace<Targetable>(root);
        rig_attachment::RecomputeRigTotals(registry, root);

        core::galaxy::WreckRecord record;
        record.position = position;
        record.modules = std::move(modules);
        record.elements = std::move(elements);
        record.lifetimeSeconds = DeathWreck{}.lifetimeSeconds;
        PromoteDeathWreck(registry, record);

        registry.emplace<RespawnPending>(root);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    TickLootDrops(registry, ctx.content, ctx.dt);
    TickElementDrops(registry, ctx.content, ctx.dt);
    TickDerelictWrecks(registry);
    TickDeathWrecks(registry, ctx.content, ctx.dt);
    HandlePlayerDeath(registry, ctx.content);
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
            registry.emplace<ElementDrop>(drop, stack.id, stack.quantity);
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
    record.elements = wreck.elements;
    record.lifetimeSeconds = wreck.lifetimeSeconds;

    registry.destroy(entity);
    return record;
}

entt::entity PromoteDeathWreck(entt::registry& registry, const core::galaxy::WreckRecord& record) {
    const entt::entity entity = registry.create();
    registry.emplace<WorldTransform>(entity, record.position, 0.0f);
    registry.emplace<WorldBody>(entity, kDeathWreckRadius, BodyKind::Wreck);
    registry.emplace<DeathWreck>(entity, record.modules, record.elements, record.lifetimeSeconds);
    return entity;
}

}  // namespace sr::space::loot_system
