#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/DamageSystem.h"
#include "shared/blueprints/Taxonomy.h"
#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

using Catch::Approx;
using sr::DamageType;
using sr::Destroyed;
using sr::EnginePropulsion;
using sr::Health;
using sr::ParentRig;
using sr::PendingDamage;
using sr::PowerSource;
using sr::Propulsion;
using sr::Rig;
using sr::ShellKind;
using sr::ShellRole;
using sr::Shield;
using sr::ShieldCoverage;
using sr::StructuralAttachment;
using sr::Targetable;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace damage_system = sr::space::damage_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt = 1.0f / 60.0f) {
    return SystemContext{world, intents, content, dt, 0};
}

// Ion's DamageTypeEffect row lives in data/base_game/damage_types.json (Law 10); the tests below
// that exercise it need the real shipped content, not the default-constructed, unloaded
// ContentLibrary the rest of this file uses (which resolves every DamageType to the default row).
sr::core::ContentLibrary LoadShippedContent() {
    sr::core::ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

}  // namespace

TEST_CASE("DamageSystem applies matching-type damage to the shield before the hull", "[damage]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 100.0f, 100.0f);
    registry.emplace<Shield>(hardpoint, 50.0f, 50.0f, DamageType::Kinetic, 10.0f, 3.5f, 0.0f);
    registry.emplace<PendingDamage>(hardpoint, 30.0f, DamageType::Kinetic, entt::null);

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Shield>(hardpoint).current == Approx(20.0f));
    CHECK(registry.get<Health>(hardpoint).current == Approx(100.0f));
    CHECK(registry.get<Shield>(hardpoint).rechargeCooldown == Approx(3.5f));
    CHECK_FALSE(registry.all_of<PendingDamage>(hardpoint));
}

TEST_CASE("DamageSystem lets mismatched damage bypass the shield entirely", "[damage]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 100.0f, 100.0f);
    registry.emplace<Shield>(hardpoint, 50.0f, 50.0f, DamageType::Kinetic, 10.0f, 3.5f, 0.0f);
    registry.emplace<PendingDamage>(hardpoint, 20.0f, DamageType::Energy, entt::null);

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Shield>(hardpoint).current == Approx(50.0f));
    CHECK(registry.get<Health>(hardpoint).current == Approx(80.0f));
}

TEST_CASE("DamageSystem's Personal shield covers only its own housing", "[damage][coverage]") {
    // The default coverage mode -- what every shield did before ShieldCoverage existed.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    const entt::entity generator = registry.create();
    registry.emplace<ParentRig>(generator, root);
    registry.emplace<WorldTransform>(generator, sr::Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Shield>(generator, 50.0f, 50.0f, DamageType::Kinetic, 0.0f, 0.0f, 0.0f,
                             ShieldCoverage::Personal, 100.0f);

    const entt::entity wing = registry.create();
    registry.emplace<ParentRig>(wing, root);
    registry.emplace<WorldTransform>(wing, sr::Vec2{10.0f, 0.0f}, 0.0f);
    registry.emplace<Health>(wing, 100.0f, 100.0f);
    registry.emplace<PendingDamage>(wing, 20.0f, DamageType::Kinetic, entt::null);

    registry.emplace<Rig>(root, std::vector<entt::entity>{generator, wing});

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Shield>(generator).current == Approx(50.0f));
    CHECK(registry.get<Health>(wing).current == Approx(80.0f));
}

TEST_CASE("DamageSystem's Bubble shield covers a neighbouring hardpoint within its radius",
          "[damage][coverage]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    const entt::entity generator = registry.create();
    registry.emplace<ParentRig>(generator, root);
    registry.emplace<WorldTransform>(generator, sr::Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Shield>(generator, 50.0f, 50.0f, DamageType::Kinetic, 0.0f, 0.0f, 0.0f,
                             ShieldCoverage::Bubble, 15.0f);

    const entt::entity wing = registry.create();
    registry.emplace<ParentRig>(wing, root);
    registry.emplace<WorldTransform>(wing, sr::Vec2{10.0f, 0.0f}, 0.0f);
    registry.emplace<Health>(wing, 100.0f, 100.0f);
    registry.emplace<PendingDamage>(wing, 20.0f, DamageType::Kinetic, entt::null);

    registry.emplace<Rig>(root, std::vector<entt::entity>{generator, wing});

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Shield>(generator).current == Approx(30.0f));
    CHECK(registry.get<Health>(wing).current == Approx(100.0f));
}

TEST_CASE("DamageSystem's Bubble shield does not cover a hardpoint outside its radius",
          "[damage][coverage]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    const entt::entity generator = registry.create();
    registry.emplace<ParentRig>(generator, root);
    registry.emplace<WorldTransform>(generator, sr::Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Shield>(generator, 50.0f, 50.0f, DamageType::Kinetic, 0.0f, 0.0f, 0.0f,
                             ShieldCoverage::Bubble, 5.0f);

    const entt::entity wing = registry.create();
    registry.emplace<ParentRig>(wing, root);
    registry.emplace<WorldTransform>(wing, sr::Vec2{10.0f, 0.0f}, 0.0f);
    registry.emplace<Health>(wing, 100.0f, 100.0f);
    registry.emplace<PendingDamage>(wing, 20.0f, DamageType::Kinetic, entt::null);

    registry.emplace<Rig>(root, std::vector<entt::entity>{generator, wing});

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Shield>(generator).current == Approx(50.0f));
    CHECK(registry.get<Health>(wing).current == Approx(80.0f));
}

TEST_CASE("DamageSystem's Conformal shield covers every hardpoint on the rig regardless of range",
          "[damage][coverage]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    const entt::entity generator = registry.create();
    registry.emplace<ParentRig>(generator, root);
    registry.emplace<WorldTransform>(generator, sr::Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Shield>(generator, 50.0f, 50.0f, DamageType::Kinetic, 0.0f, 0.0f, 0.0f,
                             ShieldCoverage::Conformal, 0.0f);

    const entt::entity tail = registry.create();
    registry.emplace<ParentRig>(tail, root);
    registry.emplace<WorldTransform>(tail, sr::Vec2{2000.0f, 0.0f}, 0.0f);
    registry.emplace<Health>(tail, 100.0f, 100.0f);
    registry.emplace<PendingDamage>(tail, 20.0f, DamageType::Kinetic, entt::null);

    registry.emplace<Rig>(root, std::vector<entt::entity>{generator, tail});

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Shield>(generator).current == Approx(30.0f));
    CHECK(registry.get<Health>(tail).current == Approx(100.0f));
}

TEST_CASE(
    "DamageSystem cascades destruction along StructuralAttachment when a structural parent dies",
    "[damage][cascade]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);

    const entt::entity chassis = registry.create();
    registry.emplace<Health>(chassis, 5.0f, 100.0f);
    registry.emplace<StructuralAttachment>(chassis, entt::null);
    registry.emplace<PendingDamage>(chassis, 10.0f, DamageType::Kinetic, entt::null);

    const entt::entity wing = registry.create();
    registry.emplace<Health>(wing, 100.0f, 100.0f);
    registry.emplace<StructuralAttachment>(wing, chassis);

    const entt::entity gun = registry.create();  // Attached to the wing, not the chassis directly.
    registry.emplace<Health>(gun, 100.0f, 100.0f);
    registry.emplace<StructuralAttachment>(gun, wing);

    registry.get<Rig>(root).children = {chassis, wing, gun};

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.all_of<Destroyed>(chassis));
    CHECK(registry.all_of<Destroyed>(wing));
    CHECK(registry.all_of<Destroyed>(gun));
    CHECK(registry.all_of<Destroyed>(root));  // Everything hung off the chassis -- no special case.
}

TEST_CASE(
    "DamageSystem leaves an unrelated hardpoint alone when a different structural branch dies",
    "[damage][cascade]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);

    const entt::entity chassis = registry.create();
    registry.emplace<Health>(chassis, 100.0f, 100.0f);
    registry.emplace<StructuralAttachment>(chassis, entt::null);

    const entt::entity wing = registry.create();
    registry.emplace<Health>(wing, 5.0f, 100.0f);
    registry.emplace<StructuralAttachment>(wing, chassis);
    registry.emplace<PendingDamage>(wing, 10.0f, DamageType::Kinetic, entt::null);

    const entt::entity gun = registry.create();  // Attached to the wing, dies with it.
    registry.emplace<Health>(gun, 100.0f, 100.0f);
    registry.emplace<StructuralAttachment>(gun, wing);

    registry.get<Rig>(root).children = {chassis, wing, gun};

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<Destroyed>(chassis));
    CHECK(registry.all_of<Destroyed>(wing));
    CHECK(registry.all_of<Destroyed>(gun));
    CHECK_FALSE(registry.all_of<Destroyed>(root));  // The chassis still lives.
}

TEST_CASE("DamageSystem spills damage exceeding shield capacity onto the hull", "[damage]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 100.0f, 100.0f);
    // rechargeCooldown starts mid-count so the regen pass this tick cannot perturb `current`
    // before the damage pass reads it.
    registry.emplace<Shield>(hardpoint, 10.0f, 50.0f, DamageType::Kinetic, 10.0f, 3.5f, 1.0f);
    registry.emplace<PendingDamage>(hardpoint, 30.0f, DamageType::Kinetic, entt::null);

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Shield>(hardpoint).current == Approx(0.0f));
    CHECK(registry.get<Health>(hardpoint).current == Approx(80.0f));
    CHECK(registry.get<Shield>(hardpoint).rechargeCooldown == Approx(3.5f));
}

TEST_CASE("DamageSystem destroys a hardpoint whose hull reaches zero", "[damage]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 5.0f, 100.0f);
    registry.emplace<PendingDamage>(hardpoint, 10.0f, DamageType::Kinetic, entt::null);

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Health>(hardpoint).current == Approx(0.0f));
    CHECK(registry.all_of<Destroyed>(hardpoint));
}

TEST_CASE("DamageSystem regenerates an undamaged shield once its cooldown has elapsed",
          "[damage]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity hardpoint = registry.create();
    registry.emplace<Shield>(hardpoint, 50.0f, 100.0f, DamageType::Kinetic, 10.0f, 3.5f, 0.0f);

    damage_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK(registry.get<Shield>(hardpoint).current == Approx(60.0f));
}

TEST_CASE("DamageSystem does not regenerate a shield while its post-hit cooldown is counting down",
          "[damage]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity hardpoint = registry.create();
    registry.emplace<Shield>(hardpoint, 50.0f, 100.0f, DamageType::Kinetic, 10.0f, 3.5f, 2.0f);

    damage_system::Tick(MakeContext(world, intents, content, 1.0f));

    CHECK(registry.get<Shield>(hardpoint).rechargeCooldown == Approx(1.0f));
    CHECK(registry.get<Shield>(hardpoint).current == Approx(50.0f));
}

TEST_CASE(
    "DamageSystem marks a rig destroyed and untargetable once every hardpoint is gone "
    "(no protected core)",
    "[damage]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<Targetable>(root);

    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 0.0f, 100.0f);
    registry.emplace<Destroyed>(hardpoint);
    registry.get<Rig>(root).children.push_back(hardpoint);

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.all_of<Destroyed>(root));
    CHECK_FALSE(registry.all_of<Targetable>(root));
}

TEST_CASE("DamageSystem zeroes a rig's propulsion once its last living engine hardpoint dies",
          "[damage]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<Propulsion>(root, 5000.0f, 3.0f, 400.0f);

    const entt::entity armor = registry.create();
    registry.emplace<Health>(armor, 50.0f, 50.0f);
    registry.emplace<ShellRole>(armor, ShellKind::Armor);

    const entt::entity engine = registry.create();
    registry.emplace<Health>(engine, 0.0f, 45.0f);
    registry.emplace<ShellRole>(engine, ShellKind::Engine);
    registry.emplace<EnginePropulsion>(engine, 5000.0f, 3.0f, 400.0f);
    registry.emplace<Destroyed>(engine);

    registry.get<Rig>(root).children = {armor, engine};

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<Destroyed>(root));
    const auto& propulsion = registry.get<Propulsion>(root);
    CHECK(propulsion.thrustNewtons == Approx(0.0f));
    CHECK(propulsion.turnTorque == Approx(0.0f));
    CHECK(propulsion.maxSpeed == Approx(0.0f));
}

TEST_CASE("DamageSystem's per-tick recompute reproduces a living engine's propulsion unchanged",
          "[damage]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<Propulsion>(root, 5000.0f, 3.0f, 400.0f);

    const entt::entity engine = registry.create();
    registry.emplace<Health>(engine, 45.0f, 45.0f);
    registry.emplace<ShellRole>(engine, ShellKind::Engine);
    registry.emplace<EnginePropulsion>(engine, 5000.0f, 3.0f, 400.0f);
    registry.get<Rig>(root).children.push_back(engine);

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Propulsion>(root).thrustNewtons == Approx(5000.0f));
}

TEST_CASE(
    "DamageSystem absorbs Kinetic damage regardless of its source hardpoint, closing the "
    "ram-vs-shield gap (architecture.md 15.1 finding 1)",
    "[damage]") {
    // CollisionSystem::ApplyRamDamage tags ram-sourced PendingDamage as DamageType::Kinetic
    // exactly the same way a projectile hit does; this reproduces that shape directly rather than
    // routing through CollisionSystem, since the fix lives entirely in DamageSystem's generic
    // per-hardpoint absorption path, not in how a hit gets tagged.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 100.0f, 100.0f);
    registry.emplace<Shield>(hardpoint, 50.0f, 50.0f, DamageType::Kinetic, 10.0f, 3.5f, 0.0f);
    registry.emplace<PendingDamage>(hardpoint, 30.0f, DamageType::Kinetic, entt::null);

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Shield>(hardpoint).current == Approx(20.0f));
    CHECK(registry.get<Health>(hardpoint).current == Approx(100.0f));
}

TEST_CASE(
    "DamageSystem lets a Kinetic hit bypass an Energy shield and deal full hull damage, "
    "unchanged from before the effect table",
    "[damage]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 100.0f, 100.0f);
    registry.emplace<Shield>(hardpoint, 50.0f, 50.0f, DamageType::Energy, 10.0f, 3.5f, 0.0f);
    registry.emplace<PendingDamage>(hardpoint, 30.0f, DamageType::Kinetic, entt::null);

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Shield>(hardpoint).current == Approx(50.0f));
    CHECK(registry.get<Health>(hardpoint).current == Approx(70.0f));
}

TEST_CASE(
    "DamageSystem's Ion effect is absorbed by a mismatched Kinetic shield, drains it, and "
    "deals zero hull damage while still suppressing power",
    "[damage]") {
    const sr::core::ContentLibrary content = LoadShippedContent();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 100.0f, 100.0f);
    registry.emplace<Shield>(hardpoint, 50.0f, 50.0f, DamageType::Kinetic, 10.0f, 3.5f, 0.0f);
    registry.emplace<PowerSource>(hardpoint, 40.0f);
    registry.emplace<PendingDamage>(hardpoint, 30.0f, DamageType::Ion, entt::null);

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Shield>(hardpoint).current == Approx(20.0f));
    CHECK(registry.get<Shield>(hardpoint).rechargeCooldown == Approx(3.5f));
    CHECK(registry.get<Health>(hardpoint).current == Approx(100.0f));
    CHECK(registry.get<PowerSource>(hardpoint).generation == Approx(10.0f));
}

TEST_CASE("DamageSystem's Ion effect is absorbed by an Energy shield the same as a Kinetic one",
          "[damage]") {
    const sr::core::ContentLibrary content = LoadShippedContent();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 100.0f, 100.0f);
    registry.emplace<Shield>(hardpoint, 50.0f, 50.0f, DamageType::Energy, 10.0f, 3.5f, 0.0f);
    registry.emplace<PowerSource>(hardpoint, 40.0f);
    registry.emplace<PendingDamage>(hardpoint, 30.0f, DamageType::Ion, entt::null);

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Shield>(hardpoint).current == Approx(20.0f));
    CHECK(registry.get<Health>(hardpoint).current == Approx(100.0f));
    CHECK(registry.get<PowerSource>(hardpoint).generation == Approx(10.0f));
}

TEST_CASE(
    "DamageSystem's Ion effect drains power directly on an unshielded hardpoint and still "
    "deals no hull damage",
    "[damage]") {
    const sr::core::ContentLibrary content = LoadShippedContent();
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;

    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 100.0f, 100.0f);
    registry.emplace<PowerSource>(hardpoint, 40.0f);
    registry.emplace<PendingDamage>(hardpoint, 20.0f, DamageType::Ion, entt::null);

    damage_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Health>(hardpoint).current == Approx(100.0f));
    CHECK(registry.get<PowerSource>(hardpoint).generation == Approx(20.0f));
}

TEST_CASE(
    "DamageSystem costs propulsion proportionally when one of two engines dies, not zeroing "
    "until the last",
    "[damage]") {
    // architecture.md 12.23's Sum/Max rule: two engines of equal thrust means half survives when
    // one dies, not the old HasLivingEngine all-or-nothing zeroing.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);
    registry.emplace<Propulsion>(root);

    const entt::entity engineA = registry.create();
    registry.emplace<Health>(engineA, 45.0f, 45.0f);
    registry.emplace<ShellRole>(engineA, ShellKind::Engine);
    registry.emplace<EnginePropulsion>(engineA, 2500.0f, 1.5f, 400.0f);

    const entt::entity engineB = registry.create();
    registry.emplace<Health>(engineB, 0.0f, 45.0f);
    registry.emplace<ShellRole>(engineB, ShellKind::Engine);
    registry.emplace<EnginePropulsion>(engineB, 2500.0f, 1.5f, 400.0f);
    registry.emplace<Destroyed>(engineB);

    registry.get<Rig>(root).children = {engineA, engineB};

    damage_system::Tick(MakeContext(world, intents, content));

    const auto& propulsion = registry.get<Propulsion>(root);
    CHECK(propulsion.thrustNewtons == Approx(2500.0f));
    CHECK(propulsion.turnTorque == Approx(1.5f));
    CHECK(propulsion.maxSpeed == Approx(400.0f));
}
