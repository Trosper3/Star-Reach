#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/economy/FactionEconomy.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/systems/StationServicesSystem.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Rig.h"
#include "shared/components/StationServices.h"
#include "shared/rig/CargoView.h"

using Catch::Approx;
using sr::BuyItemRequest;
using sr::CargoHold;
using sr::CrewRepairBonus;
using sr::Destroyed;
using sr::Docked;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::FactionId;
using sr::FactionRef;
using sr::Health;
using sr::ItemKind;
using sr::ItemStack;
using sr::ModuleDef;
using sr::ModuleId;
using sr::ModuleKind;
using sr::MountedModules;
using sr::ParentRig;
using sr::RepairOrder;
using sr::Rig;
using sr::SellItemRequest;
using sr::TransferItemRequest;
using sr::Wallet;
using sr::core::ContentLibrary;
using sr::core::economy::FactionEconomy;
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
    registry.emplace<ParentRig>(hardpoint, station);
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

TEST_CASE(
    "RepairOrder heals toward its target, capped by the facility's rate this tick, "
    "billing whole credits for hull actually restored",
    "[station-services][integration][repair]") {
    // architecture.md 13.3 finding I / 13.4 decision 1: the rate DockingSystem's deleted free
    // heal used to apply moves to FacilityStats::ratePerSecond instead of dying with it -- this
    // is that field's reader. A slow facility can only deliver rate * dt this tick.
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 60.0f);  // 60 * (1/60) = 1.0 HP achievable
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);  // 50 missing -- rate-capped, not target

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<Wallet>(rig, 100);
    registry.emplace<Rig>(rig, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairOrder>(rig, RepairOrder{rig, hardpoint, 1.0f});

    station_services_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.all_of<RepairOrder>(rig));                       // Target not yet reached.
    CHECK(registry.get<Health>(hardpoint).current == Approx(51.0f));  // +1.0 HP this tick
    CHECK(registry.get<Wallet>(rig).credits == 98);  // costPerHp(grade 1) == 2; 1.0 HP * 2 == 2
}

TEST_CASE(
    "RepairOrder's rate is multiplied by the docked STATION's own crew, not the "
    "requester's",
    "[station-services][integration][repair][crew]") {
    // features.md 2.7's Repair role: an officer multiplies the facility they are stationed
    // aboard, never whoever is being repaired -- CrewRepairBonus lives on the station root
    // (shared/rig/ModuleAttachment.cpp's RecomputeRigTotals), read here via the facility
    // hardpoint's own ParentRig.
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 60.0f);  // 60 * (1/60) == 1.0 HP achievable
    registry.emplace<CrewRepairBonus>(station, 0.5f);      // +50% from a Repair-rolled bridge crew
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    // The requester's OWN crew must not matter -- only lack of a CrewRepairBonus on `rig` itself
    // would silently pass this test, so its absence here is deliberate, not an oversight.
    registry.emplace<Wallet>(rig, 100);
    registry.emplace<Rig>(rig, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairOrder>(rig, RepairOrder{rig, hardpoint, 1.0f});

    station_services_system::Tick(MakeContext(world, intents, content));

    // Achieved this tick: 1.0 * 1.5 == 1.5 HP, not the un-boosted 1.0.
    CHECK(registry.get<Health>(hardpoint).current == Approx(51.5f));
    CHECK(registry.get<Wallet>(rig).credits == 97);  // 1.5 HP * costPerHp(2) == 3
}

TEST_CASE("RepairOrder completes and stops billing once its target fraction is reached",
          "[station-services][integration][repair]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station,
                      6000.0f);  // fast enough to reach target in 1 tick
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<Wallet>(rig, 100);
    registry.emplace<Rig>(rig, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairOrder>(rig, RepairOrder{rig, hardpoint, 0.6f});  // target 60/100

    const SystemContext ctx = MakeContext(world, intents, content);
    station_services_system::Tick(ctx);  // Reaches the target this tick.
    CHECK(registry.get<Health>(hardpoint).current == Approx(60.0f));
    CHECK(registry.get<Wallet>(rig).credits == 80);  // 10 HP * costPerHp(2) == 20

    station_services_system::Tick(ctx);  // Detects target already met -- completes, bills nothing.
    CHECK_FALSE(registry.all_of<RepairOrder>(rig));
    CHECK(registry.get<Health>(hardpoint).current == Approx(60.0f));
    CHECK(registry.get<Wallet>(rig).credits == 80);
}

TEST_CASE(
    "RepairOrder does not heal a Destroyed hardpoint even under Repair All, and charges "
    "nothing for it",
    "[station-services][integration][repair]") {
    // architecture.md 12.30.7's Destroyed sweep: a permanently dead hardpoint stays dead --
    // features.md 3.9's colour-is-condition schematic would otherwise draw it green after this.
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 6000.0f);
    const entt::entity destroyedHardpoint = registry.create();
    registry.emplace<Health>(destroyedHardpoint, 0.0f, 100.0f);
    registry.emplace<Destroyed>(destroyedHardpoint);
    const entt::entity livingHardpoint = registry.create();
    registry.emplace<Health>(livingHardpoint, 50.0f, 100.0f);

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<Wallet>(rig, 100);
    registry.emplace<Rig>(rig, std::vector<entt::entity>{destroyedHardpoint, livingHardpoint});
    registry.emplace<RepairOrder>(rig, RepairOrder{rig, entt::null, 1.0f});  // Repair All

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Health>(destroyedHardpoint).current == 0.0f);
    CHECK(registry.get<Health>(livingHardpoint).current == Approx(100.0f));
    CHECK(registry.get<Wallet>(rig).credits == 0);  // 50 HP * costPerHp(2) == 100, all from living
}

TEST_CASE(
    "RepairOrder stalls -- heals nothing, bills nothing, stays in place -- when the "
    "wallet cannot afford this tick's cost",
    "[station-services][integration][repair]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 6000.0f);
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<Wallet>(rig, 10);  // Nowhere near costPerHp(2) * 50 missing == 100
    registry.emplace<Rig>(rig, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairOrder>(rig, RepairOrder{rig, hardpoint, 1.0f});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Wallet>(rig).credits == 10);
    CHECK(registry.get<Health>(hardpoint).current == 50.0f);
    CHECK(registry.all_of<RepairOrder>(rig));  // Stalled, not gone -- may resume once funded.
}

TEST_CASE("RepairOrder is dropped when the docked station has no living Repair facility",
          "[station-services][integration][repair]") {
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
    registry.emplace<RepairOrder>(rig, RepairOrder{rig, hardpoint, 1.0f});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<RepairOrder>(rig));
    CHECK(registry.get<Wallet>(rig).credits == 100);
    CHECK(registry.get<Health>(hardpoint).current == 50.0f);
}

TEST_CASE("Destroying the Repair hardpoint mid-order stops it that tick",
          "[station-services][integration][repair]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    const entt::entity facility = AddRepairFacility(registry, content, station, 6000.0f);
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<Wallet>(rig, 100);
    registry.emplace<Rig>(rig, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairOrder>(rig, RepairOrder{rig, hardpoint, 1.0f});
    registry.emplace<Destroyed>(facility);

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<RepairOrder>(rig));
    CHECK(registry.get<Wallet>(rig).credits == 100);
    CHECK(registry.get<Health>(hardpoint).current == 50.0f);
}

TEST_CASE("Undocking mid-order drops the RepairOrder", "[station-services][integration][repair]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 6000.0f);
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);

    const entt::entity rig = registry.create();
    // No Docked -- already undocked before this tick runs.
    registry.emplace<Wallet>(rig, 100);
    registry.emplace<Rig>(rig, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairOrder>(rig, RepairOrder{rig, hardpoint, 1.0f});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<RepairOrder>(rig));
    CHECK(registry.get<Health>(hardpoint).current == 50.0f);
}

TEST_CASE("A station the requester owns is a valid subject and its own hardpoints heal",
          "[station-services][integration][repair]") {
    // architecture.md 12.30.4: the assertion that fails today for every station in the galaxy --
    // there is no reader anywhere that raises Health.current on an entity that is never Docked.
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    registry.emplace<FactionRef>(station, FactionId("aegis"));
    AddRepairFacility(registry, content, station, 6000.0f);
    const entt::entity stationHardpoint = registry.create();
    registry.emplace<Health>(stationHardpoint, 50.0f, 100.0f);
    registry.get<Rig>(station).children.push_back(stationHardpoint);

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<FactionRef>(rig, FactionId("aegis"));
    registry.emplace<Wallet>(rig, 100);
    registry.emplace<RepairOrder>(rig, RepairOrder{station, stationHardpoint, 1.0f});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Health>(stationHardpoint).current == Approx(100.0f));
    CHECK(registry.get<Wallet>(rig).credits == 0);  // 50 HP * costPerHp(2) == 100
}

TEST_CASE(
    "RepairOrder is invalidated when its subject is neither the requester nor an owned "
    "station",
    "[station-services][integration][repair]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    registry.emplace<FactionRef>(station, FactionId("kore"));  // Not the requester's faction.
    AddRepairFacility(registry, content, station, 6000.0f);

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<FactionRef>(rig, FactionId("aegis"));
    registry.emplace<Wallet>(rig, 100);
    registry.emplace<RepairOrder>(rig, RepairOrder{station, entt::null, 1.0f});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<RepairOrder>(rig));
    CHECK(registry.get<Wallet>(rig).credits == 100);
}

TEST_CASE("An NPC with no Wallet repairs against ctx.economy",
          "[station-services][integration][repair]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    FactionEconomy economy;
    economy.Deposit(FactionId("reavers"), 1000);

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 6000.0f);
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);

    const entt::entity npc = registry.create();
    registry.emplace<Docked>(npc, station, entt::null);
    registry.emplace<FactionRef>(npc, FactionId("reavers"));  // No Wallet.
    registry.emplace<Rig>(npc, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairOrder>(npc, RepairOrder{npc, hardpoint, 1.0f});

    SystemContext ctx = MakeContext(world, intents, content);
    ctx.economy = &economy;
    station_services_system::Tick(ctx);

    CHECK(registry.get<Health>(hardpoint).current == Approx(100.0f));
    CHECK(economy.Stock(FactionId("reavers")) == 900);  // 50 HP * costPerHp(2) == 100
}

TEST_CASE(
    "An NPC's repair is refused when its faction's stock cannot afford it, and heals "
    "nothing",
    "[station-services][integration][repair]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    FactionEconomy economy;
    economy.Deposit(FactionId("reavers"), 5);  // Far short of the tick's cost.

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 6000.0f);
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);

    const entt::entity npc = registry.create();
    registry.emplace<Docked>(npc, station, entt::null);
    registry.emplace<FactionRef>(npc, FactionId("reavers"));
    registry.emplace<Rig>(npc, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairOrder>(npc, RepairOrder{npc, hardpoint, 1.0f});

    SystemContext ctx = MakeContext(world, intents, content);
    ctx.economy = &economy;
    station_services_system::Tick(ctx);

    CHECK(registry.get<Health>(hardpoint).current == 50.0f);
    CHECK(economy.Stock(FactionId("reavers")) == 5);
    CHECK(registry.all_of<RepairOrder>(npc));  // Stalled, not gone.
}

TEST_CASE("An NPC does not repair, and does not repair for free, with ctx.economy == nullptr",
          "[station-services][integration][repair]") {
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 6000.0f);
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);

    const entt::entity npc = registry.create();
    registry.emplace<Docked>(npc, station, entt::null);
    registry.emplace<FactionRef>(npc, FactionId("reavers"));
    registry.emplace<Rig>(npc, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairOrder>(npc, RepairOrder{npc, hardpoint, 1.0f});

    station_services_system::Tick(MakeContext(world, intents, content));  // ctx.economy left null.

    CHECK(registry.get<Health>(hardpoint).current == 50.0f);
    CHECK(registry.all_of<RepairOrder>(npc));  // Stalled, not gone.
}

TEST_CASE("A repair spanning many ticks charges a total, not zero -- the rounding trap",
          "[station-services][integration][repair]") {
    // architecture.md 12.30.4's own warning: billing per tick in whole credits at a slow rate
    // would round to zero every tick and make repair free again unless the fractional remainder
    // is carried forward (RepairBilling, shared/components/StationServices.h).
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 6.0f);  // 6 * (1/60) == 0.1 HP/tick
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<Wallet>(rig, 100);
    registry.emplace<Rig>(rig, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairOrder>(rig, RepairOrder{rig, hardpoint, 1.0f});

    const SystemContext ctx = MakeContext(world, intents, content);
    for (int i = 0; i < 100; ++i) {
        station_services_system::Tick(ctx);
    }

    // 100 ticks * 0.1 HP == 10.0 HP restored; costPerHp(grade 1) == 2 -> 20 credits total.
    CHECK(registry.get<Health>(hardpoint).current == Approx(60.0f));
    CHECK(registry.get<Wallet>(rig).credits == 80);
}

TEST_CASE("Rapidly toggling a repair order cannot repeatedly dodge the fractional-credit carry",
          "[station-services][integration][repair]") {
    // issue #268: modes/space/ui/RepairScreen.cpp's ToggleOrder destroys and recreates
    // RepairOrder every time the player clicks the repair button. If the fractional-credit
    // remainder lived on RepairOrder itself (as it used to), each restart would reset it to
    // zero, so a per-tick charge would round down to zero forever and heal for free regardless
    // of Wallet balance. This simulates the exploit directly -- a fresh RepairOrder every tick,
    // exactly what spamming the button produces -- as a regression test for RepairBilling
    // persisting independent of that toggle.
    ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    AddRepairFacility(registry, content, station, 6.0f);  // 6 * (1/60) == 0.1 HP/tick
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);

    const entt::entity rig = registry.create();
    registry.emplace<Docked>(rig, station, entt::null);
    registry.emplace<Wallet>(rig, 0);  // Cannot afford even a single credit of repair.
    registry.emplace<Rig>(rig, std::vector<entt::entity>{hardpoint});

    const SystemContext ctx = MakeContext(world, intents, content);
    for (int i = 0; i < 100; ++i) {
        registry.emplace_or_replace<RepairOrder>(rig, RepairOrder{rig, hardpoint, 1.0f});
        station_services_system::Tick(ctx);
    }

    // The old, buggy behaviour would have restored the full 10 HP the "rounding trap" case above
    // shows 100 unthrottled ticks deliver -- for zero credits. Correct behaviour bounds any
    // rounding leak to strictly less than one credit's worth of hull, ever, then stalls for good.
    CHECK(registry.get<Health>(hardpoint).current < 51.0f);
    CHECK(registry.get<Wallet>(rig).credits == 0);
}

TEST_CASE("Deposit moves a stack from the requester's cargo to the station's, free of charge",
          "[station-services][integration][transfer]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    registry.emplace<FactionRef>(station, FactionId("aegis"));
    const entt::entity requester = registry.create();
    registry.emplace<Docked>(requester, station, entt::null);
    registry.emplace<FactionRef>(requester, FactionId("aegis"));
    GiveCargoBay(registry, requester);
    cargo_view::Deposit(registry, requester, ItemStack{ItemKind::Element, "Fe", 5, 2.0f});
    registry.emplace<TransferItemRequest>(
        requester, TransferItemRequest{ItemKind::Element, "Fe", 5, /*toStation=*/true});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<TransferItemRequest>(requester));
    CHECK(cargo_view::Merged(registry, requester).empty());
    const std::vector<ItemStack> stationCargo = cargo_view::Merged(registry, station);
    REQUIRE(stationCargo.size() == 1);
    CHECK(stationCargo.front().id == "Fe");
    CHECK(stationCargo.front().quantity == 5);
}

TEST_CASE("Withdraw moves a stack from the station's cargo to the requester's",
          "[station-services][integration][transfer]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    registry.emplace<FactionRef>(station, FactionId("aegis"));
    cargo_view::Deposit(registry, station, ItemStack{ItemKind::Element, "Fe", 5, 2.0f});
    const entt::entity requester = registry.create();
    registry.emplace<Docked>(requester, station, entt::null);
    registry.emplace<FactionRef>(requester, FactionId("aegis"));
    GiveCargoBay(registry, requester);
    registry.emplace<TransferItemRequest>(
        requester, TransferItemRequest{ItemKind::Element, "Fe", 5, /*toStation=*/false});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<TransferItemRequest>(requester));
    CHECK(cargo_view::Merged(registry, station).empty());
    const std::vector<ItemStack> requesterCargo = cargo_view::Merged(registry, requester);
    REQUIRE(requesterCargo.size() == 1);
    CHECK(requesterCargo.front().id == "Fe");
    CHECK(requesterCargo.front().quantity == 5);
}

TEST_CASE("Deposit with a targetHold set lands there, not the auto-picked emptiest bay",
          "[station-services][integration][transfer]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = registry.create();
    registry.emplace<FactionRef>(station, FactionId("aegis"));
    const entt::entity bayEmpty = GiveCargoBay(registry, station);
    const entt::entity bayChosen = registry.create();
    registry.emplace<ParentRig>(bayChosen, station);
    registry.emplace<CargoHold>(bayChosen, std::vector<ItemStack>{}, 10, 1000.0f);
    registry.replace<Rig>(station, std::vector<entt::entity>{bayEmpty, bayChosen});

    const entt::entity requester = registry.create();
    registry.emplace<Docked>(requester, station, entt::null);
    registry.emplace<FactionRef>(requester, FactionId("aegis"));
    GiveCargoBay(registry, requester);
    cargo_view::Deposit(registry, requester, ItemStack{ItemKind::Element, "Fe", 5, 2.0f});
    registry.emplace<TransferItemRequest>(
        requester, TransferItemRequest{ItemKind::Element, "Fe", 5, /*toStation=*/true, bayChosen});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<CargoHold>(bayEmpty).stacks.empty());  // The auto-pick was never reached.
    REQUIRE(registry.get<CargoHold>(bayChosen).stacks.size() == 1);
    CHECK(registry.get<CargoHold>(bayChosen).stacks.front().id == "Fe");
}

TEST_CASE("Transfer refuses across different owners and moves nothing",
          "[station-services][integration][transfer]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    registry.emplace<FactionRef>(station, FactionId("kore"));  // Not the requester's faction.
    const entt::entity requester = registry.create();
    registry.emplace<Docked>(requester, station, entt::null);
    registry.emplace<FactionRef>(requester, FactionId("aegis"));
    GiveCargoBay(registry, requester);
    cargo_view::Deposit(registry, requester, ItemStack{ItemKind::Element, "Fe", 5, 2.0f});
    registry.emplace<TransferItemRequest>(
        requester, TransferItemRequest{ItemKind::Element, "Fe", 5, /*toStation=*/true});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<TransferItemRequest>(requester));
    CHECK(cargo_view::Merged(registry, station).empty());
    CHECK(cargo_view::Merged(registry, requester).size() == 1);
}

TEST_CASE("Transfer refuses whole when the source does not hold the item",
          "[station-services][integration][transfer]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = MakeStation(registry, {});
    registry.emplace<FactionRef>(station, FactionId("aegis"));
    const entt::entity requester = registry.create();
    registry.emplace<Docked>(requester, station, entt::null);
    registry.emplace<FactionRef>(requester, FactionId("aegis"));
    GiveCargoBay(registry, requester);  // Empty -- nothing to withdraw from.
    registry.emplace<TransferItemRequest>(
        requester, TransferItemRequest{ItemKind::Element, "Fe", 5, /*toStation=*/true});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<TransferItemRequest>(requester));
    CHECK(cargo_view::Merged(registry, station).empty());
    CHECK(cargo_view::Merged(registry, requester).empty());
}

TEST_CASE("Transfer refuses whole and undoes the withdrawal when the destination has no room",
          "[station-services][integration][transfer]") {
    // architecture.md 12.30.3: "every transfer checks the destination and is refused whole,
    // never partially applied."
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity station = registry.create();
    registry.emplace<FactionRef>(station, FactionId("aegis"));
    const entt::entity stationBay = registry.create();
    registry.emplace<ParentRig>(stationBay, station);
    registry.emplace<CargoHold>(stationBay, std::vector<ItemStack>{}, 1, 1.0f);  // Barely any room.
    registry.emplace<Rig>(station, std::vector<entt::entity>{stationBay});

    const entt::entity requester = registry.create();
    registry.emplace<Docked>(requester, station, entt::null);
    registry.emplace<FactionRef>(requester, FactionId("aegis"));
    GiveCargoBay(registry, requester);
    cargo_view::Deposit(registry, requester, ItemStack{ItemKind::Element, "Fe", 5, 2.0f});
    registry.emplace<TransferItemRequest>(
        requester, TransferItemRequest{ItemKind::Element, "Fe", 5, /*toStation=*/true});

    station_services_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<TransferItemRequest>(requester));
    CHECK(cargo_view::Merged(registry, station).empty());
    const std::vector<ItemStack> requesterCargo = cargo_view::Merged(registry, requester);
    REQUIRE(requesterCargo.size() == 1);
    CHECK(requesterCargo.front().quantity == 5);  // Round-tripped, not lost.
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
