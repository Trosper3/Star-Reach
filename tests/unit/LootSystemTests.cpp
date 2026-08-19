#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "core/events/IntentQueue.h"
#include "core/galaxy/WreckRecord.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/LootSystem.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Spawn.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/rig/CargoView.h"

using sr::BodyKind;
using sr::CargoHold;
using sr::CollisionRadius;
using sr::DeathWreck;
using sr::DerelictWreck;
using sr::Destroyed;
using sr::ElementDrop;
using sr::ElementStack;
using sr::Health;
using sr::ItemKind;
using sr::ItemStack;
using sr::LootDrop;
using sr::ModuleId;
using sr::MountedModules;
using sr::ParentRig;
using sr::PlayerControlled;
using sr::PlayerLocation;
using sr::RespawnPending;
using sr::Rig;
using sr::Targetable;
using sr::Vec2;
using sr::Wallet;
using sr::WorldBody;
using sr::WorldTransform;
using sr::core::galaxy::WreckLedger;
using sr::core::galaxy::WreckRecord;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace loot_system = sr::space::loot_system;
namespace cargo_view = sr::cargo_view;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt = 1.0f / 60.0f) {
    return SystemContext{world, intents, content, dt, 0};
}

// A collector needs a living cargo-bay hardpoint to receive anything at all (architecture.md
// 12.23) -- a bare PlayerControlled entity with no Rig has nowhere to deposit into.
entt::entity MakeCollector(entt::registry& registry, const Vec2& position, float radius,
                           int slotCount = 10, float slotCapacity = 1000.0f) {
    const entt::entity collector = registry.create();
    registry.emplace<WorldTransform>(collector, position, 0.0f);
    registry.emplace<CollisionRadius>(collector, radius);
    registry.emplace<PlayerControlled>(collector);

    const entt::entity bay = registry.create();
    registry.emplace<ParentRig>(bay, collector);
    registry.emplace<CargoHold>(bay, std::vector<ItemStack>{}, slotCount, slotCapacity);
    registry.emplace<Rig>(collector, std::vector<entt::entity>{bay});

    return collector;
}

// A dead player rig: root plus two already-Destroyed hardpoints, one carrying a mounted module
// and one carrying cargo -- enough to exercise HandlePlayerDeath's manifest collection without
// going through RigFactory. PlayerLocation, not PlayerControlled -- HandlePlayerDeath identifies
// the player that way (see its own doc comment for why).
entt::entity MakeDeadPlayerRig(entt::registry& registry, const Vec2& position) {
    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, position, 0.0f);
    registry.emplace<PlayerLocation>(root, root);
    registry.emplace<Destroyed>(root);

    const entt::entity weaponBay = registry.create();
    registry.emplace<Health>(weaponBay, 0.0f, 50.0f);
    registry.emplace<Destroyed>(weaponBay);
    registry.emplace<MountedModules>(weaponBay, std::vector<ModuleId>{ModuleId("pulse_cannon_i")});

    const entt::entity cargoBay = registry.create();
    registry.emplace<Health>(cargoBay, 0.0f, 30.0f);
    registry.emplace<Destroyed>(cargoBay);
    registry.emplace<CargoHold>(
        cargoBay, std::vector<ItemStack>{ItemStack{ItemKind::Element, "Fe", 3, 2.0f}}, 4, 250.0f);

    registry.emplace<Rig>(root, std::vector<entt::entity>{weaponBay, cargoBay});
    return root;
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
    const std::vector<ItemStack> cargo = cargo_view::Merged(registry, collector);
    REQUIRE(cargo.size() == 1);
    CHECK(cargo.front().kind == ItemKind::Module);
    CHECK(cargo.front().id == "pulse_cannon_i");
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

TEST_CASE("LootSystem leaves a LootDrop alone when the collector has no cargo bay", "[loot]") {
    // Regression coverage for architecture.md 12.23: a collector with no CargoBay hardpoint has
    // nowhere to deposit into, so pickup must be refused rather than silently discarding the
    // drop or crashing on a missing CargoHold.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity collector = registry.create();
    registry.emplace<WorldTransform>(collector, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<CollisionRadius>(collector, 50.0f);
    registry.emplace<PlayerControlled>(collector);

    const entt::entity drop = registry.create();
    registry.emplace<WorldTransform>(drop, Vec2{20.0f, 0.0f}, 0.0f);
    registry.emplace<LootDrop>(drop, ModuleId("pulse_cannon_i"), 28.0f);

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(drop));
    CHECK(registry.get<LootDrop>(drop).lifetimeSeconds < 28.0f);
}

TEST_CASE(
    "LootSystem spills exactly a Destroyed cargo bay's own stacks as recoverable drops, and "
    "other bays are untouched",
    "[loot]") {
    // architecture.md 12.23: "shoot the bay, lose what was in it" -- as recoverable salvage, not
    // as nothing. DamageSystem only tags Destroyed and never destroys the hardpoint entity, so
    // the component is still here for LootSystem to read the same tick.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    const entt::entity destroyedBay = registry.create();
    registry.emplace<sr::ParentRig>(destroyedBay, root);
    registry.emplace<CargoHold>(
        destroyedBay,
        std::vector<ItemStack>{ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 14.0f},
                               ItemStack{ItemKind::Element, "Fe", 3, 2.0f}},
        4, 250.0f);
    registry.emplace<WorldTransform>(destroyedBay, Vec2{50.0f, 0.0f}, 0.0f);
    registry.emplace<sr::Destroyed>(destroyedBay);

    const entt::entity survivingBay = registry.create();
    registry.emplace<sr::ParentRig>(survivingBay, root);
    registry.emplace<CargoHold>(
        survivingBay, std::vector<ItemStack>{ItemStack{ItemKind::Element, "carbon", 1, 1.0f}}, 4,
        250.0f);

    registry.emplace<Rig>(root, std::vector<entt::entity>{destroyedBay, survivingBay});

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<CargoHold>(destroyedBay).stacks.empty());
    REQUIRE(registry.get<CargoHold>(survivingBay).stacks.size() == 1);
    CHECK(registry.get<CargoHold>(survivingBay).stacks.front().id == "carbon");

    CHECK(registry.storage<LootDrop>().size() == 1);
    CHECK(registry.storage<ElementDrop>().size() == 1);
    for (const entt::entity drop : registry.view<LootDrop>()) {
        CHECK(registry.get<LootDrop>(drop).moduleId == ModuleId("pulse_cannon_i"));
        CHECK(registry.get<WorldTransform>(drop).position == Vec2{50.0f, 0.0f});
    }
    for (const entt::entity drop : registry.view<ElementDrop>()) {
        CHECK(registry.get<ElementDrop>(drop).elementId == "Fe");
        CHECK(registry.get<ElementDrop>(drop).quantity == 3);
    }
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

TEST_CASE("LootSystem merges repeated ElementDrop pickups of the same element into one stack",
          "[loot]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity collector = MakeCollector(registry, Vec2{0.0f, 0.0f}, 50.0f);
    cargo_view::Deposit(registry, collector, ItemStack{ItemKind::Element, "Fe", 3, 2.0f});

    const entt::entity drop = registry.create();
    registry.emplace<WorldTransform>(drop, Vec2{10.0f, 0.0f}, 0.0f);
    registry.emplace<ElementDrop>(drop, "Fe", 2, 28.0f);

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.valid(drop));
    const std::vector<ItemStack> cargo = cargo_view::Merged(registry, collector);
    REQUIRE(cargo.size() == 1);
    CHECK(cargo.front().id == "Fe");
    CHECK(cargo.front().quantity == 5);
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
    deathWreck.elements.push_back(ElementStack{"Fe", 3});
    deathWreck.lifetimeSeconds = 120.0f;
    registry.emplace<DeathWreck>(wreck, deathWreck);

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.valid(wreck));
    const std::vector<ItemStack> cargo = cargo_view::Merged(registry, collector);
    REQUIRE(cargo.size() == 2);
    const bool hasModule = std::any_of(cargo.begin(), cargo.end(), [](const ItemStack& stack) {
        return stack.kind == ItemKind::Module && stack.id == "pulse_cannon_i";
    });
    const bool hasElement = std::any_of(cargo.begin(), cargo.end(), [](const ItemStack& stack) {
        return stack.kind == ItemKind::Element && stack.id == "Fe" && stack.quantity == 3;
    });
    CHECK(hasModule);
    CHECK(hasElement);
}

TEST_CASE("LootSystem despawns a DeathWreck once its recovery window expires unclaimed",
          "[loot][wreck]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity wreck = registry.create();
    registry.emplace<WorldTransform>(wreck, Vec2{9000.0f, 0.0f}, 0.0f);
    registry.emplace<DeathWreck>(wreck, std::vector<ModuleId>{}, std::vector<ElementStack>{}, 0.5f);

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
    deathWreck.elements.push_back(ElementStack{"Fe", 3});
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
    REQUIRE(promotedWreck.elements.size() == 1);
    CHECK(promotedWreck.elements.front().elementId == "Fe");
    CHECK(promotedWreck.elements.front().quantity == 3);
    CHECK(promotedWreck.lifetimeSeconds == 90.0f);
}

TEST_CASE("A demoted DeathWreck's expiry keeps counting down in its WreckRecord form",
          "[loot][wreck]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    const entt::entity original = registry.create();
    registry.emplace<WorldTransform>(original, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<DeathWreck>(original, std::vector<ModuleId>{}, std::vector<ElementStack>{},
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
    deathWreck.elements.push_back(ElementStack{"Fe", 3});
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

    const std::vector<ItemStack> cargo = cargo_view::Merged(registry, collector);
    REQUIRE(cargo.size() == 2);
    CHECK(ledger.Find(id) == nullptr);
}

TEST_CASE(
    "LootSystem revives a dead PlayerLocation rig at the death position, drops its losses as "
    "a DeathWreck, and hands SpawnSystem a RespawnPending",
    "[loot][wreck][death]") {
    // architecture.md 13.3 R: RespawnPending's only producer. features.md section 3.3: the
    // vessel's equipped modules and cargo are lost to a recoverable wreck, not deleted outright.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = MakeDeadPlayerRig(registry, Vec2{40.0f, -15.0f});
    const auto hardpoints = registry.get<Rig>(root).children;

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<Destroyed>(root));
    CHECK(registry.all_of<Targetable>(root));
    REQUIRE(registry.all_of<RespawnPending>(root));

    for (const entt::entity hardpoint : hardpoints) {
        CHECK_FALSE(registry.all_of<Destroyed>(hardpoint));
        if (const auto* health = registry.try_get<Health>(hardpoint)) {
            CHECK(health->current == health->max);
        }
        if (const auto* mounted = registry.try_get<MountedModules>(hardpoint)) {
            CHECK(mounted->ids.empty());
        }
        if (const auto* cargo = registry.try_get<CargoHold>(hardpoint)) {
            CHECK(cargo->stacks.empty());
        }
    }

    const auto wrecks = registry.view<DeathWreck>();
    REQUIRE(std::distance(wrecks.begin(), wrecks.end()) == 1);
    const entt::entity wreck = *wrecks.begin();
    CHECK(registry.get<WorldTransform>(wreck).position == Vec2{40.0f, -15.0f});
    const DeathWreck& manifest = registry.get<DeathWreck>(wreck);
    REQUIRE(manifest.modules.size() == 1);
    CHECK(manifest.modules.front() == ModuleId("pulse_cannon_i"));
    REQUIRE(manifest.elements.size() == 1);
    CHECK(manifest.elements.front().elementId == "Fe");
    CHECK(manifest.elements.front().quantity == 3);
}

TEST_CASE("LootSystem leaves a living player rig alone", "[loot][death]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<PlayerLocation>(root, root);
    registry.emplace<Rig>(root);

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<RespawnPending>(root));
    CHECK(registry.view<DeathWreck>().empty());
}

TEST_CASE("LootSystem never drops a DeathWreck for a Destroyed rig that is not PlayerControlled",
          "[loot][death]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Destroyed>(root);
    registry.emplace<Rig>(root);

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.all_of<Destroyed>(root));
    CHECK_FALSE(registry.all_of<RespawnPending>(root));
    CHECK(registry.view<DeathWreck>().empty());
}
