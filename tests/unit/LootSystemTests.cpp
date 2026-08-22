#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>

#include "core/events/IntentQueue.h"
#include "core/galaxy/WreckRecord.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/factories/RigFactory.h"
#include "modes/space/systems/LootSystem.h"
#include "shared/components/Combat.h"
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
using sr::CrewRating;
using sr::DeathWreck;
using sr::DerelictWreck;
using sr::Destroyed;
using sr::ElementDrop;
using sr::ElementStack;
using sr::Health;
using sr::ItemKind;
using sr::ItemStack;
using sr::JettisonRequest;
using sr::LootDrop;
using sr::ModuleId;
using sr::MountedModules;
using sr::ParentRig;
using sr::PlayerLocation;
using sr::Propulsion;
using sr::RespawnPending;
using sr::Rig;
using sr::Targetable;
using sr::Uncrewed;
using sr::Vec2;
using sr::Wallet;
using sr::Weapon;
using sr::WorldBody;
using sr::WorldTransform;
using sr::core::ContentLibrary;
using sr::core::galaxy::WreckLedger;
using sr::core::galaxy::WreckRecord;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace loot_system = sr::space::loot_system;
namespace cargo_view = sr::cargo_view;
namespace rig_factory = sr::space::rig_factory;

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt = 1.0f / 60.0f) {
    return SystemContext{world, intents, content, dt, 0};
}

// A collector needs a living cargo-bay hardpoint to receive anything at all (architecture.md
// 12.23) -- a bare PlayerLocation entity with no Rig has nowhere to deposit into.
entt::entity MakeCollector(entt::registry& registry, const Vec2& position, float radius,
                           int slotCount = 10, float slotCapacity = 1000.0f) {
    const entt::entity collector = registry.create();
    registry.emplace<WorldTransform>(collector, position, 0.0f);
    registry.emplace<CollisionRadius>(collector, radius);
    registry.emplace<PlayerLocation>(collector, collector);

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
    registry.emplace<PlayerLocation>(collector, collector);

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

TEST_CASE("A JettisonRequest for a held Module withdraws it and spawns a collectible LootDrop",
          "[loot]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeCollector(registry, Vec2{100.0f, 50.0f}, 50.0f);
    cargo_view::Deposit(registry, rig, ItemStack{ItemKind::Module, "pulse_cannon_i", 1, 14.0f});
    registry.emplace<JettisonRequest>(rig, JettisonRequest{ItemKind::Module, "pulse_cannon_i", 1});

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<JettisonRequest>(rig));
    CHECK(cargo_view::Merged(registry, rig).empty());  // Withdrawn from the rig's own hold.

    entt::entity drop = entt::null;
    for (const entt::entity entity : registry.view<LootDrop>()) {
        drop = entity;
    }
    REQUIRE((drop != entt::null));  // Appears as a pickup -- same LootDrop TickLootDrops collects.
    CHECK(registry.get<LootDrop>(drop).moduleId == ModuleId("pulse_cannon_i"));
    REQUIRE(registry.all_of<WorldTransform>(drop));
    CHECK(registry.get<WorldTransform>(drop).position.x == 100.0f);
    CHECK(registry.get<WorldTransform>(drop).position.y == 50.0f);
}

TEST_CASE(
    "A JettisonRequest for held Elements withdraws exactly the requested quantity and spawns an "
    "ElementDrop",
    "[loot]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeCollector(registry, Vec2{0.0f, 0.0f}, 50.0f);
    cargo_view::Deposit(registry, rig, ItemStack{ItemKind::Element, "Fe", 5, 2.0f});
    registry.emplace<JettisonRequest>(rig, JettisonRequest{ItemKind::Element, "Fe", 2});

    loot_system::Tick(MakeContext(world, intents, content));

    const std::vector<ItemStack> remaining = cargo_view::Merged(registry, rig);
    REQUIRE(remaining.size() == 1);
    CHECK(remaining.front().quantity == 3);  // 5 held, 2 jettisoned.

    entt::entity drop = entt::null;
    for (const entt::entity entity : registry.view<ElementDrop>()) {
        drop = entity;
    }
    REQUIRE((drop != entt::null));
    CHECK(registry.get<ElementDrop>(drop).elementId == "Fe");
    CHECK(registry.get<ElementDrop>(drop).quantity == 2);
}

TEST_CASE("A JettisonRequest for more than is held is refused and drops nothing", "[loot]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeCollector(registry, Vec2{0.0f, 0.0f}, 50.0f);
    cargo_view::Deposit(registry, rig, ItemStack{ItemKind::Element, "Fe", 2, 2.0f});
    registry.emplace<JettisonRequest>(rig, JettisonRequest{ItemKind::Element, "Fe", 5});

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<JettisonRequest>(rig));
    const std::vector<ItemStack> remaining = cargo_view::Merged(registry, rig);
    REQUIRE(remaining.size() == 1);
    CHECK(remaining.front().quantity == 2);  // Untouched -- refused, not partially withdrawn.
    CHECK(registry.view<ElementDrop>().empty());
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

TEST_CASE(
    "A dead, blueprint-spawned player rig respawns with its default loadout re-armed, not bare",
    "[loot][death]") {
    // Full-pipeline regression: a real aegis_vanguard has a BlueprintRef RigFactory writes at
    // spawn, which HandlePlayerDeath now uses to re-mount the starter loadout per hardpoint
    // instead of leaving the hull stripped -- MakeDeadPlayerRig's hand-built fixture above has no
    // BlueprintRef, so it can't exercise this path.
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    rig_factory::SpawnParams params;
    params.blueprint = sr::BlueprintId("aegis_vanguard");
    params.position = {0.0f, 0.0f};
    const auto spawned = rig_factory::Spawn(world, content, params);
    REQUIRE(spawned.ok());

    // RigFactory itself is faction/player-agnostic (NPCs go through it too) -- PlayerLocation is
    // written by SpaceFlight::SpawnPlayerInto at a higher layer, not here.
    registry.emplace<PlayerLocation>(spawned.root, spawned.root);
    // Mirrors DamageSystem's own death transition (DamageSystem.cpp): Targetable comes off before
    // Destroyed goes on. HandlePlayerDeath's unconditional emplace<Targetable> assumes that
    // ordering already happened -- skipping it here (unlike the real combat path) would double-
    // emplace and abort.
    registry.remove<Targetable>(spawned.root);
    registry.emplace<Destroyed>(spawned.root);
    for (const entt::entity hardpoint : registry.get<Rig>(spawned.root).children) {
        registry.emplace<Destroyed>(hardpoint);
    }

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<Destroyed>(spawned.root));
    REQUIRE(registry.all_of<RespawnPending>(spawned.root));

    const auto port = rig_factory::FindHardpoint(registry, spawned.root, sr::MountId("wing_port"));
    REQUIRE(registry.valid(port));
    REQUIRE(registry.all_of<Weapon>(port));
    REQUIRE(registry.all_of<MountedModules>(port));
    REQUIRE(registry.get<MountedModules>(port).ids.size() == 1);
    CHECK(registry.get<MountedModules>(port).ids.front() == ModuleId("pulse_cannon_i"));

    const auto thruster =
        rig_factory::FindHardpoint(registry, spawned.root, sr::MountId("thruster_main"));
    REQUIRE(registry.valid(thruster));
    REQUIRE(registry.all_of<MountedModules>(thruster));
    CHECK(registry.get<MountedModules>(thruster).ids.front() == ModuleId("ion_thruster_i"));

    const auto hold = rig_factory::FindHardpoint(registry, spawned.root, sr::MountId("hold"));
    REQUIRE(registry.valid(hold));
    CHECK(registry.get<CargoHold>(hold).stacks.empty());

    // Re-armed, not just re-mounted -- Propulsion is derived from EnginePropulsion, so a real
    // thrust/turn capability confirms the engine's role components came back too, not just its
    // MountedModules bookkeeping.
    REQUIRE(registry.all_of<Propulsion>(spawned.root));
    CHECK(registry.get<Propulsion>(spawned.root).thrustNewtons > 0.0f);
}

TEST_CASE(
    "An Uncrewed player rig ends the run the same way a fully Destroyed one does, and respawn "
    "re-crews the cockpit",
    "[loot][death]") {
    // features.md 3.2: losing the crew shell is fatal for the player even though the rest of the
    // hull is untouched -- Uncrewed alone must trigger the same respawn path Destroyed does.
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    rig_factory::SpawnParams params;
    params.blueprint = sr::BlueprintId("aegis_vanguard");
    params.position = {0.0f, 0.0f};
    const auto spawned = rig_factory::Spawn(world, content, params);
    REQUIRE(spawned.ok());

    registry.emplace<PlayerLocation>(spawned.root, spawned.root);

    const auto cockpit = rig_factory::FindHardpoint(registry, spawned.root, sr::MountId("cockpit"));
    REQUIRE(registry.valid(cockpit));
    REQUIRE(registry.all_of<CrewRating>(cockpit));
    registry.get<Health>(cockpit).current = 0.0f;
    registry.emplace<Destroyed>(cockpit);
    // What DamageSystem's RecomputeRigTotals would have tagged this same tick, ahead of
    // LootSystem in SystemSchedule.cpp -- exercised here in isolation.
    registry.emplace<Uncrewed>(spawned.root);

    // The rest of the rig is untouched -- unlike the full-death fixture above, the root itself
    // was never Destroyed.
    CHECK_FALSE(registry.all_of<Destroyed>(spawned.root));

    loot_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.all_of<RespawnPending>(spawned.root));
    CHECK_FALSE(registry.all_of<Uncrewed>(spawned.root));
    REQUIRE(registry.all_of<CrewRating>(cockpit));  // re-crewed from the starter loadout
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

TEST_CASE(
    "LootSystem leaves a wreck and reaps a combat-killed rig that has nothing mounted or held",
    "[loot][death]") {
    // P2-09: a combat kill produces exactly one DeathWreck even with an empty hold, and the
    // destroyed root is gone from the registry on the following tick rather than sitting there
    // forever for every other system to keep iterating.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{70.0f, -5.0f}, 0.0f);
    registry.emplace<Destroyed>(root);
    registry.emplace<Rig>(root);

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.valid(root));
    CHECK_FALSE(registry.all_of<RespawnPending>(root));

    const auto wrecks = registry.view<DeathWreck>();
    REQUIRE(std::distance(wrecks.begin(), wrecks.end()) == 1);
    const entt::entity wreck = *wrecks.begin();
    CHECK(registry.get<WorldTransform>(wreck).position == Vec2{70.0f, -5.0f});
    CHECK(registry.get<DeathWreck>(wreck).modules.empty());
}

TEST_CASE(
    "LootSystem spills a combat-killed rig's cargo as loose pickups and its mounted modules as "
    "a DeathWreck manifest, then reaps every hardpoint",
    "[loot][death]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{50.0f, 0.0f}, 0.0f);
    registry.emplace<Destroyed>(root);

    const entt::entity weaponBay = registry.create();
    registry.emplace<WorldTransform>(weaponBay, Vec2{50.0f, 0.0f}, 0.0f);
    registry.emplace<Destroyed>(weaponBay);
    registry.emplace<MountedModules>(weaponBay, std::vector<ModuleId>{ModuleId("pulse_cannon_i")});

    const entt::entity cargoBay = registry.create();
    registry.emplace<WorldTransform>(cargoBay, Vec2{50.0f, 0.0f}, 0.0f);
    registry.emplace<Destroyed>(cargoBay);
    registry.emplace<CargoHold>(
        cargoBay, std::vector<ItemStack>{ItemStack{ItemKind::Element, "Fe", 3, 2.0f}}, 4, 250.0f);

    registry.emplace<Rig>(root, std::vector<entt::entity>{weaponBay, cargoBay});

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.valid(root));
    CHECK_FALSE(registry.valid(weaponBay));
    CHECK_FALSE(registry.valid(cargoBay));

    // Cargo spills loose, the same producer a single destroyed bay already uses -- it is not
    // folded into the wreck manifest.
    REQUIRE(registry.storage<ElementDrop>().size() == 1);
    for (const entt::entity drop : registry.view<ElementDrop>()) {
        CHECK(registry.get<ElementDrop>(drop).elementId == "Fe");
        CHECK(registry.get<ElementDrop>(drop).quantity == 3);
    }
    CHECK(registry.storage<LootDrop>().size() == 0);

    const auto wrecks = registry.view<DeathWreck>();
    REQUIRE(std::distance(wrecks.begin(), wrecks.end()) == 1);
    const DeathWreck& manifest = registry.get<DeathWreck>(*wrecks.begin());
    REQUIRE(manifest.modules.size() == 1);
    CHECK(manifest.modules.front() == ModuleId("pulse_cannon_i"));
}

TEST_CASE("LootSystem leaves a PlayerLocation rig's death entirely to HandlePlayerDeath",
          "[loot][death]") {
    // Regression coverage for the exclude<PlayerLocation> guard: without it, the same Destroyed
    // rig would be processed twice -- revived by HandlePlayerDeath and reaped by
    // HandleCombatKills in the same tick.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = MakeDeadPlayerRig(registry, Vec2{40.0f, -15.0f});

    loot_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(root));
    CHECK_FALSE(registry.all_of<Destroyed>(root));
    REQUIRE(registry.all_of<RespawnPending>(root));

    const auto wrecks = registry.view<DeathWreck>();
    REQUIRE(std::distance(wrecks.begin(), wrecks.end()) == 1);
}
