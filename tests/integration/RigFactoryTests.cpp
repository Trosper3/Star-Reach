#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>

#include "core/registries/ContentLibrary.h"
#include "modes/space/factories/RigFactory.h"
#include "shared/blueprints/Validation.h"
#include "shared/components/Combat.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

using Catch::Approx;
using sr::core::ContentLibrary;
using sr::space::SystemWorld;
namespace factory = sr::space::rig_factory;

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

factory::SpawnParams VanguardAt(float x, float y) {
    factory::SpawnParams params;
    params.blueprint = sr::BlueprintId("aegis_vanguard");
    params.position = {x, y};
    return params;
}

factory::SpawnParams OutpostAt(float x, float y) {
    factory::SpawnParams params;
    params.blueprint = sr::BlueprintId("aegis_outpost");
    params.position = {x, y};
    return params;
}

}  // namespace

TEST_CASE("A blueprint becomes a parent entity plus one child per mount", "[factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    const auto result = factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    const auto& rig = world.Registry().get<sr::Rig>(result.root);
    // aegis_vanguard declares seven mounts: core, reactor, thruster, two wings, emitter, hold.
    CHECK(rig.children.size() == 7);

    for (const entt::entity child : rig.children) {
        CHECK(world.Registry()
                  .all_of<sr::ParentRig, sr::MountRef, sr::Health, sr::LocalTransform,
                          sr::WorldTransform, sr::HitRadius>(child));
        CHECK(world.Registry().get<sr::ParentRig>(child).root == result.root);
    }
}

TEST_CASE("Role components land on the hardpoints that earn them", "[factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    const auto result = factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();

    // Two weapon hardpoints, each with its OWN cooldown. This is the property that makes
    // losing one turret lose exactly one firing arc (features.md section 3.2).
    const auto port = factory::FindHardpoint(registry, result.root, sr::MountId("wing_port"));
    const auto starboard =
        factory::FindHardpoint(registry, result.root, sr::MountId("wing_starboard"));
    REQUIRE(registry.all_of<sr::Weapon>(port));
    REQUIRE(registry.all_of<sr::Weapon>(starboard));
    CHECK(port != starboard);
    CHECK(registry.get<sr::Weapon>(port).damage == Approx(15.0f));

    const auto emitter = factory::FindHardpoint(registry, result.root, sr::MountId("emitter"));
    REQUIRE(registry.all_of<sr::Shield>(emitter));
    CHECK(registry.get<sr::Shield>(emitter).absorbs == sr::DamageType::Kinetic);

    const auto reactor = factory::FindHardpoint(registry, result.root, sr::MountId("reactor"));
    REQUIRE(registry.all_of<sr::PowerSource>(reactor));
    CHECK(registry.get<sr::PowerSource>(reactor).generation == Approx(140.0f));

    // The chassis carries armor only: no weapon, no shield, no generation.
    const auto core = factory::FindHardpoint(registry, result.root, sr::MountId("core"));
    CHECK_FALSE(registry.any_of<sr::Weapon, sr::Shield, sr::PowerSource>(core));
}

TEST_CASE("Thrust aggregates onto the rig, not onto the engine hardpoint", "[factory]") {
    // PhysicsSystem reads one Propulsion on the root. DamageSystem recomputes it when an engine
    // hardpoint dies -- which is how "destroy the thruster and the ship stalls" falls out of the
    // data rather than needing a special case per craft type.
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    const auto result = factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();
    REQUIRE(registry.all_of<sr::Propulsion>(result.root));
    CHECK(registry.get<sr::Propulsion>(result.root).thrustNewtons == Approx(5200.0f));

    const auto thruster =
        factory::FindHardpoint(registry, result.root, sr::MountId("thruster_main"));
    CHECK_FALSE(registry.all_of<sr::Propulsion>(thruster));
}

TEST_CASE(
    "A mobile: false blueprint still gets Propulsion and LinearDamping, aggregating to zero "
    "thrust",
    "[factory]") {
    // Regression test for architecture.md 12.25: RigFactory used to emplace no Propulsion (and
    // no LinearDamping) at all for a mobile: false blueprint, rather than a zeroed one --
    // "capability is emergent" means every root gets both, and a station's aggregate stays zero
    // because nothing in its mount list is an engine, not because a blueprint flag excluded it.
    // aegis_outpost is this content set's only mobile: false blueprint and has no engine mount,
    // so this also doubles as the finding-J regression: LinearDamping is what stops it
    // accelerating without bound near the sun (see OrbitSystemTests.cpp for that scenario).
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    const auto result = factory::Spawn(world, content, OutpostAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();
    REQUIRE(registry.all_of<sr::Propulsion>(result.root));
    CHECK(registry.get<sr::Propulsion>(result.root).thrustNewtons == Approx(0.0f));
    REQUIRE(registry.all_of<sr::LinearDamping>(result.root));
    CHECK(registry.get<sr::LinearDamping>(result.root).perSecond == Approx(0.35f));
}

TEST_CASE("A rig with no sensor module has zero sensor range, not the old hardcoded 2000",
          "[factory]") {
    // Regression test for architecture.md 12.23: RigFactory used to emplace SensorRange hardcoded
    // at 2000.0f on every root regardless of content -- a hardcoded producer with nothing
    // authored behind it, the same shape as FiringArc::turnRatePerSecond's old kPi. aegis_vanguard
    // mounts no Sensor module, so its SensorRange is now emergent, not authored: zero.
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    const auto result = factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();
    REQUIRE(registry.all_of<sr::SensorRange>(result.root));
    CHECK(registry.get<sr::SensorRange>(result.root).units == Approx(0.0f));
}

TEST_CASE("Rig mass is the sum of every shell and module", "[factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    const auto result = factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    // Mass is the real denominator in F = ma, so the loadout puzzle has teeth
    // (features.md section 2.2).
    const sr::ShipBlueprint* blueprint = content.FindShip(sr::BlueprintId("aegis_vanguard"));
    REQUIRE(blueprint != nullptr);
    const sr::RigTotals totals = sr::ComputeTotals(*blueprint, content);

    CHECK(world.Registry().get<sr::BodyMass>(result.root).kilograms == Approx(totals.mass));
}

TEST_CASE("Hardpoint world transforms are seeded relative to the rig root", "[factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    const auto result = factory::Spawn(world, content, VanguardAt(100.0f, 50.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();
    const auto port = factory::FindHardpoint(registry, result.root, sr::MountId("wing_port"));

    // wing_port sits at (8.5, -14.7224) in rig space; the rig is at (100, 50) unrotated.
    const auto& transform = registry.get<sr::WorldTransform>(port);
    CHECK(transform.position.x == Approx(108.5f));
    CHECK(transform.position.y == Approx(35.2776f));
}

TEST_CASE("Structural attachment resolves mount ids to handles", "[factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    const auto result = factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();
    const auto core = factory::FindHardpoint(registry, result.root, sr::MountId("core"));
    const auto port = factory::FindHardpoint(registry, result.root, sr::MountId("wing_port"));

    CHECK(registry.get<sr::StructuralAttachment>(port).attachedTo == core);
    // The root mount attaches to nothing.
    CHECK((registry.get<sr::StructuralAttachment>(core).attachedTo == entt::null));
}

TEST_CASE("An unknown blueprint spawns nothing at all", "[factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    factory::SpawnParams params;
    params.blueprint = sr::BlueprintId("no_such_ship");

    const auto result = factory::Spawn(world, content, params);
    CHECK_FALSE(result.ok());
    // No half-built rig left behind for a system to trip over.
    CHECK(world.Registry().storage<sr::Rig>().size() == 0);
}

TEST_CASE("The rig is trackable by stable network id", "[factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    const auto result = factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    CHECK(result.networkId != sr::space::kInvalidNetworkId);
    CHECK((world.Resolve(result.networkId) == result.root));
}

TEST_CASE("Weapon hardpoints sharing a ModuleId share a weapon group", "[factory]") {
    // aegis_vanguard's two wing mounts both carry pulse_cannon_i (features.md 3.6: "each distinct
    // weapon ModuleId takes the next free group").
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    const auto result = factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();
    const auto port = factory::FindHardpoint(registry, result.root, sr::MountId("wing_port"));
    const auto starboard =
        factory::FindHardpoint(registry, result.root, sr::MountId("wing_starboard"));

    REQUIRE(registry.all_of<sr::WeaponGroup>(port));
    REQUIRE(registry.all_of<sr::WeaponGroup>(starboard));
    CHECK(registry.get<sr::WeaponGroup>(port).index ==
          registry.get<sr::WeaponGroup>(starboard).index);

    // Non-weapon hardpoints never get a group at all.
    const auto core = factory::FindHardpoint(registry, result.root, sr::MountId("core"));
    CHECK_FALSE(registry.all_of<sr::WeaponGroup>(core));
}

TEST_CASE("A freshly spawned rig has every weapon group enabled", "[factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    const auto result = factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    REQUIRE(result.ok());

    const entt::registry& registry = world.Registry();
    REQUIRE(registry.all_of<sr::EnabledWeaponGroups>(result.root));
    CHECK(registry.get<sr::EnabledWeaponGroups>(result.root).mask == 0x03FFu);
}

TEST_CASE("Two spawns from one blueprint are independent rigs", "[factory]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    const auto a = factory::Spawn(world, content, VanguardAt(0.0f, 0.0f));
    const auto b = factory::Spawn(world, content, VanguardAt(500.0f, 0.0f));
    REQUIRE(a.ok());
    REQUIRE(b.ok());

    CHECK(a.root != b.root);
    CHECK(a.networkId != b.networkId);

    const auto& rigA = world.Registry().get<sr::Rig>(a.root);
    const auto& rigB = world.Registry().get<sr::Rig>(b.root);
    for (const entt::entity child : rigA.children) {
        CHECK(std::find(rigB.children.begin(), rigB.children.end(), child) == rigB.children.end());
    }
}
