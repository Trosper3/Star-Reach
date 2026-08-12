#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/galaxy/WreckRecord.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/LootSystem.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Physics.h"
#include "shared/components/Transform.h"

using sr::BodyKind;
using sr::CargoHold;
using sr::CollisionRadius;
using sr::DeathWreck;
using sr::DerelictWreck;
using sr::LootDrop;
using sr::MaterialDrop;
using sr::MaterialStack;
using sr::ModuleId;
using sr::PlayerControlled;
using sr::Vec2;
using sr::Wallet;
using sr::WorldBody;
using sr::WorldTransform;
using sr::core::galaxy::WreckLedger;
using sr::core::galaxy::WreckRecord;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace loot_system = sr::space::loot_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt = 1.0f / 60.0f) {
    return SystemContext{world, intents, content, dt, 0};
}

entt::entity MakeCollector(entt::registry& registry, const Vec2& position, float radius) {
    const entt::entity collector = registry.create();
    registry.emplace<WorldTransform>(collector, position, 0.0f);
    registry.emplace<CollisionRadius>(collector, radius);
    registry.emplace<PlayerControlled>(collector);
    return collector;
}

}  // namespace

TEST_CASE("LootSystem collects a LootDrop within the collector's pickup radius", "[loot]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity collector = MakeCollector(registry, Vec2{0.0f, 0.0f}, 50.0f);
    const entt::entity drop = registry.create();
    registry.emplace<WorldTransform>(drop, Vec2{20.0f, 0.0f}, 0.0f);
    registry.emplace<LootDrop>(drop, ModuleId("pulse_cannon_i"), 28.0f);

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.valid(drop));
    REQUIRE(registry.all_of<CargoHold>(collector));
    const CargoHold& cargo = registry.get<CargoHold>(collector);
    REQUIRE(cargo.modules.size() == 1);
    CHECK(cargo.modules.front() == ModuleId("pulse_cannon_i"));
}

TEST_CASE("LootSystem leaves a LootDrop alone when no collector is in range", "[loot]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    MakeCollector(registry, Vec2{0.0f, 0.0f}, 50.0f);
    const entt::entity drop = registry.create();
    registry.emplace<WorldTransform>(drop, Vec2{9000.0f, 0.0f}, 0.0f);
    registry.emplace<LootDrop>(drop, ModuleId("pulse_cannon_i"), 28.0f);

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(drop));
    CHECK(registry.get<LootDrop>(drop).lifetimeSeconds < 28.0f);
}

TEST_CASE("LootSystem despawns a LootDrop once its lifetime expires unclaimed", "[loot]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity drop = registry.create();
    registry.emplace<WorldTransform>(drop, Vec2{9000.0f, 0.0f}, 0.0f);
    registry.emplace<LootDrop>(drop, ModuleId("pulse_cannon_i"), 0.5f);

    loot_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK_FALSE(registry.valid(drop));
}

TEST_CASE("LootSystem merges repeated MaterialDrop pickups of the same material into one stack",
          "[loot]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity collector = MakeCollector(registry, Vec2{0.0f, 0.0f}, 50.0f);
    registry.emplace<CargoHold>(collector).materials.push_back({"Fe", 3});

    const entt::entity drop = registry.create();
    registry.emplace<WorldTransform>(drop, Vec2{10.0f, 0.0f}, 0.0f);
    registry.emplace<MaterialDrop>(drop, "Fe", 2, 28.0f);

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.valid(drop));
    const CargoHold& cargo = registry.get<CargoHold>(collector);
    REQUIRE(cargo.materials.size() == 1);
    CHECK(cargo.materials.front().materialId == "Fe");
    CHECK(cargo.materials.front().quantity == 5);
}

TEST_CASE("LootSystem credits a collector's Wallet on DerelictWreck salvage and destroys it",
          "[loot]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity collector = MakeCollector(registry, Vec2{0.0f, 0.0f}, 30.0f);
    const entt::entity wreck = registry.create();
    // Outside the collector's own 30-unit radius, but within reach once the wreck's own 70-unit
    // radiusUnits (isCapital) is added -- exactly the "player radius + wreck radius" rule ported
    // from legacy StarReach2.
    registry.emplace<WorldTransform>(wreck, Vec2{90.0f, 0.0f}, 0.0f);
    registry.emplace<DerelictWreck>(wreck, 2500, true, 70.0f);

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.valid(wreck));
    REQUIRE(registry.all_of<Wallet>(collector));
    CHECK(registry.get<Wallet>(collector).credits == 2500);
}

TEST_CASE("LootSystem never expires a DerelictWreck on its own", "[loot]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity wreck = registry.create();
    registry.emplace<WorldTransform>(wreck, Vec2{9000.0f, 0.0f}, 0.0f);
    registry.emplace<DerelictWreck>(wreck, 2500, true, 70.0f);

    for (int i = 0; i < 120; ++i) {
        loot_system::Tick(MakeContext(world, intents, content, 60.0f));
    }

    CHECK(registry.valid(wreck));
}

TEST_CASE("LootSystem grants a DeathWreck's manifest to a collector in range and destroys it",
          "[loot][wreck]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity collector = MakeCollector(registry, Vec2{0.0f, 0.0f}, 50.0f);
    const entt::entity wreck = registry.create();
    registry.emplace<WorldTransform>(wreck, Vec2{20.0f, 0.0f}, 0.0f);
    DeathWreck deathWreck;
    deathWreck.modules.push_back(ModuleId("pulse_cannon_i"));
    deathWreck.materials.push_back(MaterialStack{"Fe", 3});
    deathWreck.lifetimeSeconds = 120.0f;
    registry.emplace<DeathWreck>(wreck, deathWreck);

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.valid(wreck));
    REQUIRE(registry.all_of<CargoHold>(collector));
    const CargoHold& cargo = registry.get<CargoHold>(collector);
    REQUIRE(cargo.modules.size() == 1);
    CHECK(cargo.modules.front() == ModuleId("pulse_cannon_i"));
    REQUIRE(cargo.materials.size() == 1);
    CHECK(cargo.materials.front().materialId == "Fe");
    CHECK(cargo.materials.front().quantity == 3);
}

TEST_CASE("LootSystem despawns a DeathWreck once its recovery window expires unclaimed",
          "[loot][wreck]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity wreck = registry.create();
    registry.emplace<WorldTransform>(wreck, Vec2{9000.0f, 0.0f}, 0.0f);
    registry.emplace<DeathWreck>(wreck, std::vector<ModuleId>{}, std::vector<MaterialStack>{},
                                 0.5f);

    loot_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK_FALSE(registry.valid(wreck));
}

TEST_CASE("CollapseDeathWreck/PromoteDeathWreck round-trip a DeathWreck's contents intact",
          "[loot][wreck]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    const entt::entity original = registry.create();
    registry.emplace<WorldTransform>(original, Vec2{42.0f, -7.0f}, 0.0f);
    DeathWreck deathWreck;
    deathWreck.modules.push_back(ModuleId("pulse_cannon_i"));
    deathWreck.materials.push_back(MaterialStack{"Fe", 3});
    deathWreck.lifetimeSeconds = 90.0f;
    registry.emplace<DeathWreck>(original, deathWreck);

    const WreckRecord record = loot_system::CollapseDeathWreck(registry, original, "sol");
    CHECK_FALSE(registry.valid(original));
    CHECK(record.systemId == "sol");
    CHECK(record.position == Vec2{42.0f, -7.0f});

    const entt::entity promoted = loot_system::PromoteDeathWreck(registry, record);

    REQUIRE(registry.valid(promoted));
    CHECK(registry.get<WorldTransform>(promoted).position == Vec2{42.0f, -7.0f});
    REQUIRE(registry.all_of<WorldBody>(promoted));
    CHECK(registry.get<WorldBody>(promoted).kind == BodyKind::Wreck);
    const DeathWreck& promotedWreck = registry.get<DeathWreck>(promoted);
    REQUIRE(promotedWreck.modules.size() == 1);
    CHECK(promotedWreck.modules.front() == ModuleId("pulse_cannon_i"));
    REQUIRE(promotedWreck.materials.size() == 1);
    CHECK(promotedWreck.materials.front().materialId == "Fe");
    CHECK(promotedWreck.materials.front().quantity == 3);
    CHECK(promotedWreck.lifetimeSeconds == 90.0f);
}

TEST_CASE("A demoted DeathWreck's expiry keeps counting down in its WreckRecord form",
          "[loot][wreck]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    const entt::entity original = registry.create();
    registry.emplace<WorldTransform>(original, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<DeathWreck>(original, std::vector<ModuleId>{}, std::vector<MaterialStack>{},
                                 5.0f);

    WreckLedger ledger;
    const WreckLedger::Id id =
        ledger.Add(loot_system::CollapseDeathWreck(registry, original, "sol"));

    ledger.Tick(3.0f);
    REQUIRE(ledger.Find(id) != nullptr);
    CHECK(ledger.Find(id)->lifetimeSeconds < 5.0f);

    ledger.Tick(3.0f);
    CHECK(ledger.Find(id) == nullptr);
}

TEST_CASE("Recovering a promoted DeathWreck grants its manifest and clears its WreckRecord",
          "[loot][wreck]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity original = registry.create();
    registry.emplace<WorldTransform>(original, Vec2{20.0f, 0.0f}, 0.0f);
    DeathWreck deathWreck;
    deathWreck.modules.push_back(ModuleId("pulse_cannon_i"));
    deathWreck.materials.push_back(MaterialStack{"Fe", 3});
    registry.emplace<DeathWreck>(original, deathWreck);

    WreckLedger ledger;
    const WreckLedger::Id id =
        ledger.Add(loot_system::CollapseDeathWreck(registry, original, "sol"));

    // The system is resident again (Tier 1): promote the record back into a live entity, and the
    // ledger's copy is no longer the source of truth.
    const WreckRecord* record = ledger.Find(id);
    REQUIRE(record != nullptr);
    loot_system::PromoteDeathWreck(registry, *record);
    ledger.Remove(id);

    const entt::entity collector = MakeCollector(registry, Vec2{0.0f, 0.0f}, 50.0f);
    loot_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.all_of<CargoHold>(collector));
    const CargoHold& cargo = registry.get<CargoHold>(collector);
    REQUIRE(cargo.modules.size() == 1);
    CHECK(cargo.modules.front() == ModuleId("pulse_cannon_i"));
    REQUIRE(cargo.materials.size() == 1);
    CHECK(cargo.materials.front().materialId == "Fe");
    CHECK(ledger.Find(id) == nullptr);
}
