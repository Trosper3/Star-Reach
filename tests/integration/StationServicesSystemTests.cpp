#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/registries/ContentLibrary.h"
#include "modes/space/systems/StationServicesSystem.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/components/StationServices.h"
#include "shared/rig/CargoView.h"

using sr::BuyItemRequest;
using sr::CargoHold;
using sr::Destroyed;
using sr::Docked;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::Health;
using sr::ItemKind;
using sr::ItemStack;
using sr::ModuleDef;
using sr::ModuleId;
using sr::ModuleKind;
using sr::MountedModules;
using sr::ParentRig;
using sr::RepairRequest;
using sr::Rig;
using sr::SellItemRequest;
using sr::Wallet;
using sr::core::ContentLibrary;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace station_services_system = sr::space::station_services_system;
namespace cargo_view = sr::cargo_view;

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

// CargoHold lives per cargo-bay hardpoint now (architecture.md 12.23) -- an owner needs a living
// Rig with a bay hardpoint under it BEFORE anything can be deposited into it (cargo_view::Deposit
// walks Rig::children). Generously sized so none of these tests are limited by slot mechanics --
// P0-10's slot/mass refusal tests live in CargoViewTests.cpp instead.
entt::entity GiveCargoBay(entt::registry& registry, entt::entity owner) {
    const entt::entity bay = registry.create();
    registry.emplace<ParentRig>(bay, owner);
    registry.emplace<CargoHold>(bay, std::vector<ItemStack>{}, 10, 1000.0f);
    registry.emplace<Rig>(owner, std::vector<entt::entity>{bay});
    return bay;
}

void StockCargo(entt::registry& registry, entt::entity owner,
                const std::vector<sr::ModuleId>& stock) {
    for (const sr::ModuleId& moduleId : stock) {
        cargo_view::Deposit(registry, owner, ItemStack{ItemKind::Module, moduleId.str(), 1, 0.0f});
    }
}

entt::entity MakeStation(entt::registry& registry, const std::vector<sr::ModuleId>& stock) {
    const entt::entity station = registry.create();
    GiveCargoBay(registry, station);
    StockCargo(registry, station, stock);
    return station;
}

// Registers a synthetic FacilityKind::Repair module at `ratePerSecond` (RegisterCraftedModule --
// the same runtime-content path EngineerSystem uses for merged modules -- rather than authoring
// one in data/base_game/modules.json, since no shipped content names FacilityKind::Repair yet)
// and attaches it to a hardpoint appended onto `station`'s existing Rig. `station` must already
// carry a Rig (MakeStation's GiveCargoBay gives it one).
entt::entity AddRepairFacility(entt::registry& registry, ContentLibrary& content,
                               entt::entity station, float ratePerSecond) {
    ModuleDef module;
    module.id = ModuleId("test_repair_bay");
    module.kind = ModuleKind::Facility;
    module.facility.kind = FacilityKind::Repair;
    module.facility.ratePerSecond = ratePerSecond;
    content.RegisterCraftedModule(module);

    const entt::entity hardpoint = registry.create();
    registry.emplace<FacilityRef>(hardpoint, FacilityKind::Repair);
    registry.emplace<MountedModules>(hardpoint, std::vector<ModuleId>{module.id});
    registry.get<Rig>(station).children.push_back(hardpoint);
    return hardpoint;
}

}  // namespace

TEST_CASE("Buying debits Wallet and moves the module from station stock to the buyer's cargo",
          "[station-services][integration]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {sr::ModuleId("pulse_cannon_i")});
    const entt::entity buyer = registry.create();
    registry.emplace<Docked>(buyer, station, entt::null);
    registry.emplace<Wallet>(buyer, 500);
    GiveCargoBay(registry, buyer);
    registry.emplace<BuyItemRequest>(buyer, BuyItemRequest{sr::ModuleId("pulse_cannon_i"), 200});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<BuyItemRequest>(buyer));
    CHECK(registry.get<Wallet>(buyer).credits == 300);
    const std::vector<ItemStack> buyerCargo = cargo_view::Merged(registry, buyer);
    REQUIRE(buyerCargo.size() == 1);
    CHECK(buyerCargo.front().id == "pulse_cannon_i");
    CHECK(cargo_view::Merged(registry, station).empty());
}

TEST_CASE("Buying refuses when the wallet cannot afford the cost",
          "[station-services][integration]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {sr::ModuleId("pulse_cannon_i")});
    const entt::entity buyer = registry.create();
    registry.emplace<Docked>(buyer, station, entt::null);
    registry.emplace<Wallet>(buyer, 50);
    GiveCargoBay(registry, buyer);
    registry.emplace<BuyItemRequest>(buyer, BuyItemRequest{sr::ModuleId("pulse_cannon_i"), 200});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Wallet>(buyer).credits == 50);
    CHECK(cargo_view::Merged(registry, buyer).empty());
    CHECK(cargo_view::Merged(registry, station).size() == 1);
}

TEST_CASE("Buying refuses when the station does not stock the module",
          "[station-services][integration]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    const entt::entity buyer = registry.create();
    registry.emplace<Docked>(buyer, station, entt::null);
    registry.emplace<Wallet>(buyer, 500);
    GiveCargoBay(registry, buyer);
    registry.emplace<BuyItemRequest>(buyer, BuyItemRequest{sr::ModuleId("pulse_cannon_i"), 200});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Wallet>(buyer).credits == 500);
    CHECK(cargo_view::Merged(registry, buyer).empty());
}

TEST_CASE("Selling is the exact inverse of buying", "[station-services][integration]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    const entt::entity seller = registry.create();
    registry.emplace<Docked>(seller, station, entt::null);
    registry.emplace<Wallet>(seller, 100);
    GiveCargoBay(registry, seller);
    StockCargo(registry, seller, {sr::ModuleId("pulse_cannon_i")});
    registry.emplace<SellItemRequest>(seller, SellItemRequest{sr::ModuleId("pulse_cannon_i"), 150});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<SellItemRequest>(seller));
    CHECK(registry.get<Wallet>(seller).credits == 250);
    CHECK(cargo_view::Merged(registry, seller).empty());
    const std::vector<ItemStack> stationCargo = cargo_view::Merged(registry, station);
    REQUIRE(stationCargo.size() == 1);
    CHECK(stationCargo.front().id == "pulse_cannon_i");
}

TEST_CASE("Selling refuses when the seller does not hold the module",
          "[station-services][integration]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    const entt::entity seller = registry.create();
    registry.emplace<Docked>(seller, station, entt::null);
    registry.emplace<Wallet>(seller, 100);
    GiveCargoBay(registry, seller);
    registry.emplace<SellItemRequest>(seller, SellItemRequest{sr::ModuleId("pulse_cannon_i"), 150});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Wallet>(seller).credits == 100);
    CHECK(cargo_view::Merged(registry, station).empty());
}

TEST_CASE("Repair spend scales with the requested fraction and heals proportionally",
          "[station-services][integration]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 1000.0f);  // fast enough to not cap this tick
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);  // 50 missing

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<Wallet>(rig, 100);
    registry.emplace<Rig>(rig, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairRequest>(rig, RepairRequest{0.5f, 100});  // spend 50 for half repair

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<RepairRequest>(rig));
    CHECK(registry.get<Wallet>(rig).credits == 50);
    CHECK(registry.get<Health>(hardpoint).current == 75.0f);  // 50 + 0.5 * 50 missing
}

TEST_CASE("Repair does not heal a Destroyed hardpoint", "[station-services][integration]") {
    // architecture.md 12.30.7's Destroyed sweep: a permanently dead hardpoint stays dead --
    // features.md 3.9's colour-is-condition schematic would otherwise draw it green after this.
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 1000.0f);  // fast enough to not cap this tick
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 0.0f, 100.0f);
    registry.emplace<Destroyed>(hardpoint);

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<Wallet>(rig, 100);
    registry.emplace<Rig>(rig, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairRequest>(rig, RepairRequest{1.0f, 100});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Health>(hardpoint).current == 0.0f);
    // The credits still spend -- the request named a fraction of the whole rig, and the point of
    // this test is the destroyed hardpoint's own health, not the wallet math.
}

TEST_CASE("Repair refuses when the wallet cannot afford the scaled spend",
          "[station-services][integration]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 1000.0f);  // fast enough to not cap this tick
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<Wallet>(rig, 10);
    registry.emplace<Rig>(rig, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairRequest>(rig, RepairRequest{0.5f, 100});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Wallet>(rig).credits == 10);
    CHECK(registry.get<Health>(hardpoint).current == 50.0f);
}

TEST_CASE("Repair refuses when the docked station has no Repair facility",
          "[station-services][integration]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});  // no repair facility attached
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<Wallet>(rig, 100);
    registry.emplace<Rig>(rig, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairRequest>(rig, RepairRequest{1.0f, 100});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<RepairRequest>(rig));
    CHECK(registry.get<Wallet>(rig).credits == 100);
    CHECK(registry.get<Health>(hardpoint).current == 50.0f);
}

TEST_CASE("Repair rate scales with the facility's authored rate",
          "[station-services][integration]") {
    // architecture.md 13.3 finding I / 13.4 decision 1: the rate DockingSystem's deleted free
    // heal used to apply moves to FacilityStats::ratePerSecond instead of dying with it -- this
    // is that field's first real reader. A slow facility can only deliver rate * dt of the
    // requested fraction in one tick, and billing follows the same capped fraction.
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 6.0f);  // 6.0 * (1/60) = 0.1 achievable fraction
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);  // 50 missing

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<Wallet>(rig, 100);
    registry.emplace<Rig>(rig, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairRequest>(rig, RepairRequest{1.0f, 100});  // asks for a full repair

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<RepairRequest>(rig));
    CHECK(registry.get<Wallet>(rig).credits == 90);           // spend = round(0.1 * 100)
    CHECK(registry.get<Health>(hardpoint).current == 55.0f);  // 50 + 0.1 * 50 missing
}

TEST_CASE("Requests are refused when the requester is not Docked",
          "[station-services][integration]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity buyer = registry.create();
    registry.emplace<Wallet>(buyer, 500);
    GiveCargoBay(registry, buyer);
    registry.emplace<BuyItemRequest>(buyer, BuyItemRequest{sr::ModuleId("pulse_cannon_i"), 200});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<BuyItemRequest>(buyer));
    CHECK(registry.get<Wallet>(buyer).credits == 500);
}
