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

using Catch::Approx;
using sr::CargoHold;
using sr::Docked;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::MergeModulesRequest;
using sr::Rig;
using sr::core::ContentLibrary;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace engineer_system = sr::space::engineer_system;

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
    CargoHold cargo;
    cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));
    cargo.modules.push_back(sr::ModuleId("autocannon_i"));
    registry.emplace<CargoHold>(requester, cargo);
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
    CHECK(registry.get<CargoHold>(requester).modules.size() == 1);
    const sr::ModuleId mergedId = registry.get<CargoHold>(requester).modules.front();
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
    CargoHold cargo;
    cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));
    cargo.modules.push_back(sr::ModuleId("autocannon_i"));
    registry.emplace<CargoHold>(requester, cargo);
    registry.emplace<MergeModulesRequest>(
        requester,
        MergeModulesRequest{sr::ModuleId("pulse_cannon_i"), sr::ModuleId("autocannon_i")});

    engineer_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<CargoHold>(requester).modules.size() == 2);
}

TEST_CASE("Merging is refused when the docked station has no Engineering facility", "[engineer]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = registry.create();  // no Rig, no facility
    const entt::entity requester = registry.create();
    registry.emplace<Docked>(requester, station, entt::null);
    CargoHold cargo;
    cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));
    cargo.modules.push_back(sr::ModuleId("autocannon_i"));
    registry.emplace<CargoHold>(requester, cargo);
    registry.emplace<MergeModulesRequest>(
        requester,
        MergeModulesRequest{sr::ModuleId("pulse_cannon_i"), sr::ModuleId("autocannon_i")});

    engineer_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<CargoHold>(requester).modules.size() == 2);
}

TEST_CASE("Merging is refused when the two modules are not the same ModuleKind", "[engineer]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeEngineeringStation(registry, 3);
    const entt::entity requester = registry.create();
    registry.emplace<Docked>(requester, station, entt::null);
    CargoHold cargo;
    cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));  // weapon
    cargo.modules.push_back(sr::ModuleId("deflector_i"));     // shield_generator
    registry.emplace<CargoHold>(requester, cargo);
    registry.emplace<MergeModulesRequest>(
        requester,
        MergeModulesRequest{sr::ModuleId("pulse_cannon_i"), sr::ModuleId("deflector_i")});

    engineer_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<CargoHold>(requester).modules.size() == 2);
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
        CargoHold cargo;
        cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));
        registry.emplace<CargoHold>(requester, cargo);
        registry.emplace<MergeModulesRequest>(
            requester,
            MergeModulesRequest{sr::ModuleId("pulse_cannon_i"), sr::ModuleId("pulse_cannon_i")});

        engineer_system::Tick(MakeContext(world, intents, content));

        CHECK(registry.get<CargoHold>(requester).modules.size() == 1);
    }

    SECTION("two copies owned -- succeeds, both consumed") {
        CargoHold cargo;
        cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));
        cargo.modules.push_back(sr::ModuleId("pulse_cannon_i"));
        registry.emplace<CargoHold>(requester, cargo);
        registry.emplace<MergeModulesRequest>(
            requester,
            MergeModulesRequest{sr::ModuleId("pulse_cannon_i"), sr::ModuleId("pulse_cannon_i")});

        engineer_system::Tick(MakeContext(world, intents, content));

        CHECK(registry.get<CargoHold>(requester).modules.size() == 1);
    }
}
