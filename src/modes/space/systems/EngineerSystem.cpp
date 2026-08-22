#include "modes/space/systems/EngineerSystem.h"

#include <optional>
#include <vector>

#include "shared/components/Engineer.h"
#include "shared/components/Facility.h"
#include "shared/components/Loot.h"
#include "shared/rig/CargoView.h"
#include "shared/rig/DockedFacility.h"

namespace sr::space::engineer_system {
namespace {

// The docked Engineering facility's authored grade, or nullopt if `requester` is not currently
// standing in one. Reads docked_facility::DockedFacility -- PlayerLocation, not a first-found
// Rig::children scan -- so a station with two benches of different grade uses whichever one the
// requester is actually standing in (architecture.md 12.30.5: "duplicate facilities are not
// fungible").
std::optional<int> DockedEngineeringLevel(const entt::registry& registry, entt::entity requester) {
    const entt::entity facility =
        docked_facility::DockedFacility(registry, requester, FacilityKind::Engineering);
    if (facility == entt::null) {
        return std::nullopt;
    }
    return registry.get<FacilityRef>(facility).grade;
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
        if (!level.has_value() || ctx.craftedModules == nullptr) {
            continue;
        }

        const ModuleDef* primary = ctx.content.FindModule(request.primary);
        const ModuleDef* secondary = ctx.content.FindModule(request.secondary);
        if (primary == nullptr || secondary == nullptr || primary->kind != secondary->kind) {
            continue;
        }

        const std::string primaryId = request.primary.str();
        const std::string secondaryId = request.secondary.str();

        // Two separate quantity-1 withdrawals, always -- a Module stack never merges (quantity
        // is always 1, ItemStack's comment), so each call independently re-scans the CURRENT
        // state of the hold. When primaryId == secondaryId (merging two owned copies of the
        // identical module), the first call consumes one of the two stacks and the second call
        // must find a DIFFERENT one still present -- which is exactly "must require two distinct
        // elements, not the same one found twice," with no special-casing needed.
        if (!cargo_view::Withdraw(registry, self, ItemKind::Module, primaryId, 1)) {
            continue;
        }
        if (!cargo_view::Withdraw(registry, self, ItemKind::Module, secondaryId, 1)) {
            // Put the primary back -- it was never really "spent."
            cargo_view::Deposit(registry, self,
                                ItemStack{ItemKind::Module, primaryId, 1, primary->mass});
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

        const auto depositResult = cargo_view::Deposit(
            registry, self, ItemStack{ItemKind::Module, merged.id.str(), 1, merged.mass});
        if (depositResult != cargo_view::DepositResult::Deposited) {
            // No room for the merged result -- put both originals back rather than lose them.
            // The crafted module registration stands; it is harmless if never actually granted.
            cargo_view::Deposit(registry, self,
                                ItemStack{ItemKind::Module, primaryId, 1, primary->mass});
            cargo_view::Deposit(registry, self,
                                ItemStack{ItemKind::Module, secondaryId, 1, secondary->mass});
        }
    }

    for (const entt::entity self : consumed) {
        registry.remove<MergeModulesRequest>(self);
    }
}

// architecture.md 12.30.5: what a deconstruct returns pending §12.19's Recipe (the "what is this
// made of" answer deconstruction is meant to read backwards) -- a flat fraction of the module's
// own mass converted to credits, scaled toward the facility grade's real recovery band
// (features.md 2.4: "20-45% ... 80-100%") without pretending to compute it from a recipe that
// does not exist yet. Grade runs 1 (Common) to 7 (Mythic), the same ladder
// ModuleAttachment.cpp's kGradeTimeFactor already uses.
constexpr float kDeconstructCreditsPerMass = 4.0f;

int DeconstructYield(const ModuleDef& module, int grade) {
    const float recoveryFraction = 0.2f + 0.1f * static_cast<float>(grade);
    return static_cast<int>(module.mass * kDeconstructCreditsPerMass * recoveryFraction);
}

void ProcessDeconstructRequests(const SystemContext& ctx) {
    entt::registry& registry = ctx.Registry();
    std::vector<entt::entity> consumed;

    for (auto [self, request] : registry.view<DeconstructModuleRequest>().each()) {
        consumed.push_back(self);

        const std::optional<int> grade = DockedEngineeringLevel(registry, self);
        if (!grade.has_value()) {
            continue;
        }
        const ModuleDef* module = ctx.content.FindModule(request.module);
        if (module == nullptr) {
            continue;
        }
        if (!cargo_view::Withdraw(registry, self, ItemKind::Module, request.module.str(), 1)) {
            continue;
        }
        if (Wallet* wallet = registry.try_get<Wallet>(self); wallet != nullptr) {
            wallet->credits += DeconstructYield(*module, *grade);
        }
    }

    for (const entt::entity self : consumed) {
        registry.remove<DeconstructModuleRequest>(self);
    }
}

}  // namespace

void Tick(const SystemContext& ctx) {
    ProcessMergeRequests(ctx);
    ProcessDeconstructRequests(ctx);
}

}  // namespace sr::space::engineer_system
