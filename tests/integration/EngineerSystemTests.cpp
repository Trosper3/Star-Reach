#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/registries/ContentLibrary.h"
#include "modes/space/systems/EngineerSystem.h"
#include "shared/components/Docking.h"
#include "shared/components/Engineer.h"
#include "shared/components/Facility.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"

using Catch::Approx;
using sr::CargoHold;
using sr::Docked;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::ItemKind;
using sr::ItemStack;
using sr::MergeModulesRequest;
using sr::ParentRig;
using sr::Rig;
using sr::core::ContentLibrary;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace engineer_system = sr::space::engineer_system;
namespace cargo_view = sr::cargo_view;

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          ContentLibrary& content, bool wireCraftedModules = true) {
    SystemContext ctx{world, intents, content, 1.0f / 60.0f, 0};
    if (wireCraftedModules) {
        ctx.craftedModules = &content;
    }
    return ctx;
}

entt::entity MakeEngineeringStation(entt::registry& registry, int level) {
    const entt::entity hardpoint = registry.create();
    registry.emplace<FacilityRef>(hardpoint, FacilityKind::Engineering, level);
    const entt::entity station = registry.create();
    registry.emplace<Rig>(station, std::vector<entt::entity>{hardpoint});
    return station;
}

// CargoHold lives per cargo-bay hardpoint now (architecture.md 12.23) -- `owner` needs a living
// Rig with a bay hardpoint before anything can be stocked into it.
entt::entity GiveCargoBay(entt::registry& registry, entt::entity owner) {
    const entt::entity bay = registry.create();
    registry.emplace<ParentRig>(bay, owner);
    registry.emplace<CargoHold>(bay, std::vector<ItemStack>{}, 10, 1000.0f);
    registry.emplace<Rig>(owner, std::vector<entt::entity>{bay});
    return bay;
}

// Deposits each module id as its own stack (Module stacks never merge, ItemStack's comment) --
// stocking the same id twice creates two distinct owned copies, matching CargoHold's pre-P0-10
// `modules.push_back` behavior.
void StockModules(entt::registry& registry, entt::entity owner, const ContentLibrary& content,
                  const std::vector<sr::ModuleId>& moduleIds) {
    for (const sr::ModuleId& moduleId : moduleIds) {
        const sr::ModuleDef* module = content.FindModule(moduleId);
        const float mass = module != nullptr ? module->mass : 0.0f;
        cargo_view::Deposit(registry, owner, ItemStack{ItemKind::Module, moduleId.str(), 1, mass});
    }
}

}  // namespace

TEST_CASE(
    "A successful merge scales the secondary module's stats by engineer level and "
    "registers a resolvable module",
    "[engineer]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeEngineeringStation(registry, 3);
    const entt::entity requester = registry.create();
    registry.emplace<Docked>(requester, station, entt::null);
    GiveCargoBay(registry, requester);
    StockModules(registry, requester, content,
                 {sr::ModuleId("pulse_cannon_i"), sr::ModuleId("autocannon_i")});
    registry.emplace<MergeModulesRequest>(
        requester,
        MergeModulesRequest{sr::ModuleId("pulse_cannon_i"), sr::ModuleId("autocannon_i")});

    const sr::ModuleDef* primary = content.FindModule(sr::ModuleId("pulse_cannon_i"));
    const sr::ModuleDef* secondary = content.FindModule(sr::ModuleId("autocannon_i"));
    REQUIRE(primary != nullptr);
    REQUIRE(secondary != nullptr);
    const float expectedDamage = primary->weapon.damage + secondary->weapon.damage * (3 * 0.1f);

    engineer_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<MergeModulesRequest>(requester));
    const std::vector<ItemStack> cargo = cargo_view::Merged(registry, requester);
    REQUIRE(cargo.size() == 1);
    const sr::ModuleId mergedId(cargo.front().id);
    const sr::ModuleDef* merged = content.FindModule(mergedId);
    REQUIRE(merged != nullptr);
    CHECK(merged->kind == sr::ModuleKind::Weapon);
    CHECK(merged->weapon.damage == Approx(expectedDamage));
}

TEST_CASE("Merging is refused when the requester is not Docked", "[engineer]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity requester = registry.create();
    GiveCargoBay(registry, requester);
    StockModules(registry, requester, content,
                 {sr::ModuleId("pulse_cannon_i"), sr::ModuleId("autocannon_i")});
    registry.emplace<MergeModulesRequest>(
        requester,
        MergeModulesRequest{sr::ModuleId("pulse_cannon_i"), sr::ModuleId("autocannon_i")});

    engineer_system::Tick(MakeContext(world, intents, content));

    CHECK(cargo_view::Merged(registry, requester).size() == 2);
}

TEST_CASE("Merging is refused when the docked station has no Engineering facility", "[engineer]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = registry.create();  // no Rig, no facility
    const entt::entity requester = registry.create();
    registry.emplace<Docked>(requester, station, entt::null);
    GiveCargoBay(registry, requester);
    StockModules(registry, requester, content,
                 {sr::ModuleId("pulse_cannon_i"), sr::ModuleId("autocannon_i")});
    registry.emplace<MergeModulesRequest>(
        requester,
        MergeModulesRequest{sr::ModuleId("pulse_cannon_i"), sr::ModuleId("autocannon_i")});

    engineer_system::Tick(MakeContext(world, intents, content));

    CHECK(cargo_view::Merged(registry, requester).size() == 2);
}

TEST_CASE("Merging is refused when the two modules are not the same ModuleKind", "[engineer]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeEngineeringStation(registry, 3);
    const entt::entity requester = registry.create();
    registry.emplace<Docked>(requester, station, entt::null);
    GiveCargoBay(registry, requester);
    StockModules(registry, requester, content,
                 {sr::ModuleId("pulse_cannon_i"),  // weapon
                  sr::ModuleId("deflector_i")});   // shield_generator
    registry.emplace<MergeModulesRequest>(
        requester,
        MergeModulesRequest{sr::ModuleId("pulse_cannon_i"), sr::ModuleId("deflector_i")});

    engineer_system::Tick(MakeContext(world, intents, content));

    CHECK(cargo_view::Merged(registry, requester).size() == 2);
}

TEST_CASE("Merging the same module id with itself requires two distinct owned copies",
          "[engineer]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeEngineeringStation(registry, 2);
    const entt::entity requester = registry.create();
    registry.emplace<Docked>(requester, station, entt::null);

    SECTION("only one copy owned -- refused") {
        GiveCargoBay(registry, requester);
        StockModules(registry, requester, content, {sr::ModuleId("pulse_cannon_i")});
        registry.emplace<MergeModulesRequest>(
            requester,
            MergeModulesRequest{sr::ModuleId("pulse_cannon_i"), sr::ModuleId("pulse_cannon_i")});

        engineer_system::Tick(MakeContext(world, intents, content));

        CHECK(cargo_view::Merged(registry, requester).size() == 1);
    }

    SECTION("two copies owned -- succeeds, both consumed") {
        GiveCargoBay(registry, requester);
        StockModules(registry, requester, content,
                     {sr::ModuleId("pulse_cannon_i"), sr::ModuleId("pulse_cannon_i")});
        registry.emplace<MergeModulesRequest>(
            requester,
            MergeModulesRequest{sr::ModuleId("pulse_cannon_i"), sr::ModuleId("pulse_cannon_i")});

        engineer_system::Tick(MakeContext(world, intents, content));

        CHECK(cargo_view::Merged(registry, requester).size() == 1);
    }
}
