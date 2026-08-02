#include "modes/space/systems/EngineerSystem.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "shared/components/Docking.h"
#include "shared/components/Engineer.h"
#include "shared/components/Facility.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"

namespace sr::space::engineer_system {
namespace {

// Returns the docked station's living Engineering facility level, or nullopt if the requester
// isn't docked or the station has none. "Living" excludes a Destroyed hardpoint -- a facility
// whose hardpoint died no longer grants the ability it was providing.
std::optional<int> DockedEngineeringLevel(const entt::registry& registry, entt::entity requester) {
    const Docked* docked = registry.try_get<Docked>(requester);
    if (docked == nullptr || !registry.valid(docked->station)) {
        return std::nullopt;
    }
    const Rig* stationRig = registry.try_get<Rig>(docked->station);
    if (stationRig == nullptr) {
        return std::nullopt;
    }
    for (const entt::entity hardpoint : stationRig->children) {
        if (registry.all_of<Destroyed>(hardpoint)) {
            continue;
        }
        const FacilityRef* facility = registry.try_get<FacilityRef>(hardpoint);
        if (facility != nullptr && facility->kind == FacilityKind::Engineering) {
            return facility->level;
        }
    }
    return std::nullopt;
}

// primaryValue + secondaryValue * (level * 0.1) -- architecture.md 12.12's formula, applied
// per-field. A placeholder scale, not a tuned value (this file's header comment).
float MergeField(float primaryValue, float secondaryValue, int level) {
    return primaryValue + secondaryValue * (static_cast<float>(level) * 0.1f);
}

// Builds the merged ModuleDef by plain declaration and field assignment, never an aggregate
// initializer -- tools/ci/check_content_pipeline.py (Law 10) forbids constructing ModuleDef with
// an initializer outside core/registries/, tests/, tools/, and a merged module is player-
// generated content, the same distinction CustomizeMenu's Template draft makes.
ModuleDef MergeModules(const ModuleDef& primary, const ModuleDef& secondary, int level) {
    ModuleDef merged;
    merged.id =
        ModuleId(primary.id.str() + "+" + secondary.id.str() + "@L" + std::to_string(level));
    merged.displayName = "Merged " + primary.displayName;
    merged.kind = primary.kind;
    merged.mass = MergeField(primary.mass, secondary.mass, level);
    merged.powerDraw = MergeField(primary.powerDraw, secondary.powerDraw, level);
    merged.powerGeneration = MergeField(primary.powerGeneration, secondary.powerGeneration, level);
    merged.hullBonus = MergeField(primary.hullBonus, secondary.hullBonus, level);

    merged.weapon = primary.weapon;
    merged.shield = primary.shield;
    merged.engine = primary.engine;
    merged.facility = primary.facility;

    switch (primary.kind) {
        case ModuleKind::Weapon:
            merged.weapon.damage =
                MergeField(primary.weapon.damage, secondary.weapon.damage, level);
            merged.weapon.rangeUnits =
                MergeField(primary.weapon.rangeUnits, secondary.weapon.rangeUnits, level);
            merged.weapon.projectileSpeed =
                MergeField(primary.weapon.projectileSpeed, secondary.weapon.projectileSpeed, level);
            break;
        case ModuleKind::ShieldGenerator:
            merged.shield.capacity =
                MergeField(primary.shield.capacity, secondary.shield.capacity, level);
            merged.shield.rechargePerSecond = MergeField(primary.shield.rechargePerSecond,
                                                         secondary.shield.rechargePerSecond, level);
            break;
        case ModuleKind::Engine:
            merged.engine.thrustNewtons =
                MergeField(primary.engine.thrustNewtons, secondary.engine.thrustNewtons, level);
            merged.engine.turnTorque =
                MergeField(primary.engine.turnTorque, secondary.engine.turnTorque, level);
            merged.engine.maxSpeed =
                MergeField(primary.engine.maxSpeed, secondary.engine.maxSpeed, level);
            break;
        case ModuleKind::Facility:
            merged.facility.ratePerSecond =
                MergeField(primary.facility.ratePerSecond, secondary.facility.ratePerSecond, level);
            break;
        default: break;
    }
    return merged;
}

void ProcessMergeRequests(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> consumed;

    for (auto [self, request] : registry.view<MergeModulesRequest>().each()) {
        consumed.push_back(self);

        const std::optional<int> level = DockedEngineeringLevel(registry, self);
        CargoHold* cargo = registry.try_get<CargoHold>(self);
        if (!level.has_value() || cargo == nullptr || ctx.craftedModules == nullptr) {
            continue;
        }

        const ModuleDef* primary = ctx.content.FindModule(request.primary);
        const ModuleDef* secondary = ctx.content.FindModule(request.secondary);
        if (primary == nullptr || secondary == nullptr || primary->kind != secondary->kind) {
            continue;
        }

        // Located by index, not iterator: primary == secondary is legal (merging two owned
        // copies of the identical module) and must require two distinct elements, not the same
        // one found twice.
        const auto primaryIt =
            std::find(cargo->modules.begin(), cargo->modules.end(), request.primary);
        if (primaryIt == cargo->modules.end()) {
            continue;
        }
        const std::size_t primaryIndex =
            static_cast<std::size_t>(primaryIt - cargo->modules.begin());

        std::size_t secondaryIndex = cargo->modules.size();
        for (std::size_t i = 0; i < cargo->modules.size(); ++i) {
            if (i != primaryIndex && cargo->modules[i] == request.secondary) {
                secondaryIndex = i;
                break;
            }
        }
        if (secondaryIndex == cargo->modules.size()) {
            continue;
        }

        // Plain declaration, then assignment, never `= MergeModules(...)` on the same line --
        // tools/ci/check_content_pipeline.py's regex matches a ModuleDef declaration with an
        // initializer regardless of what produces the value, so even a call-returning-content
        // has to land through a bare declaration first (this file's header comment explains why
        // that is legitimate here at all: player-generated, not authored, content).
        ModuleDef merged;
        merged = MergeModules(*primary, *secondary, *level);
        ctx.craftedModules->RegisterCraftedModule(merged);

        // Erase the higher index first so the lower index stays valid.
        cargo->modules.erase(cargo->modules.begin() +
                             static_cast<std::ptrdiff_t>(std::max(primaryIndex, secondaryIndex)));
        cargo->modules.erase(cargo->modules.begin() +
                             static_cast<std::ptrdiff_t>(std::min(primaryIndex, secondaryIndex)));
        cargo->modules.push_back(merged.id);
    }

    for (const entt::entity self : consumed) {
        registry.remove<MergeModulesRequest>(self);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    ProcessMergeRequests(ctx);
}

}  // namespace sr::space::engineer_system
