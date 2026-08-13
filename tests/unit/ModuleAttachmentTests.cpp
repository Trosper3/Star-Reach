#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Loot.h"
#include "shared/components/Physics.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/math/Angle.h"
#include "shared/rig/ModuleAttachment.h"

using Catch::Approx;
using sr::BodyMass;
using sr::CargoHold;
using sr::Destroyed;
using sr::EnginePropulsion;
using sr::FireControl;
using sr::HardpointMass;
using sr::HardpointSensorRange;
using sr::ModuleDef;
using sr::ModuleKind;
using sr::Propulsion;
using sr::Rig;
using sr::SensorRange;
using sr::rig_attachment::AttachModuleComponents;
using sr::rig_attachment::DetachModuleComponents;
using sr::rig_attachment::RecomputeRigTotals;

TEST_CASE("AttachModuleComponents writes Weapon/FiringArc for a weapon module", "[module-attach]") {
    entt::registry registry;
    const entt::entity hardpoint = registry.create();
    ModuleDef weapon;
    weapon.kind = ModuleKind::Weapon;
    weapon.weapon.damage = 15.0f;

    const auto propulsion = AttachModuleComponents(registry, hardpoint, weapon, 1.5f);

    REQUIRE(registry.all_of<sr::Weapon>(hardpoint));
    CHECK(registry.get<sr::Weapon>(hardpoint).damage == 15.0f);
    REQUIRE(registry.all_of<sr::FiringArc>(hardpoint));
    CHECK(registry.get<sr::FiringArc>(hardpoint).halfWidthRadians == 1.5f);
    CHECK_FALSE(propulsion.present);
}

TEST_CASE("AttachModuleComponents reports a Propulsion contribution for an engine module",
          "[module-attach]") {
    entt::registry registry;
    const entt::entity hardpoint = registry.create();
    ModuleDef engine;
    engine.kind = ModuleKind::Engine;
    engine.engine.thrustNewtons = 500.0f;

    const auto propulsion = AttachModuleComponents(registry, hardpoint, engine, 0.0f);

    CHECK(propulsion.present);
    CHECK(propulsion.thrustNewtons == 500.0f);
    CHECK_FALSE(registry.any_of<sr::Weapon, sr::Shield, sr::FacilityRef>(hardpoint));
    // Also cached on the hardpoint itself, not just returned -- RecomputeRigTotals reads this
    // back later without needing the ModuleDef (and therefore ContentLibrary) again.
    REQUIRE(registry.all_of<EnginePropulsion>(hardpoint));
    CHECK(registry.get<EnginePropulsion>(hardpoint).thrustNewtons == 500.0f);
}

TEST_CASE("AttachModuleComponents accumulates the module's mass onto HardpointMass",
          "[module-attach]") {
    entt::registry registry;
    const entt::entity hardpoint = registry.create();
    registry.emplace<HardpointMass>(hardpoint, 10.0f);  // RigFactory's shell-mass seed.
    ModuleDef armor;
    armor.kind = ModuleKind::Armor;
    armor.mass = 4.0f;

    AttachModuleComponents(registry, hardpoint, armor, 0.0f);

    CHECK(registry.get<HardpointMass>(hardpoint).value == Approx(14.0f));
}

TEST_CASE("AttachModuleComponents caches a Sensor module's range as HardpointSensorRange",
          "[module-attach]") {
    entt::registry registry;
    const entt::entity hardpoint = registry.create();
    ModuleDef sensor;
    sensor.kind = ModuleKind::Sensor;
    sensor.sensor.range = 2000.0f;

    AttachModuleComponents(registry, hardpoint, sensor, 0.0f);

    REQUIRE(registry.all_of<HardpointSensorRange>(hardpoint));
    CHECK(registry.get<HardpointSensorRange>(hardpoint).value == Approx(2000.0f));
}

TEST_CASE(
    "AttachModuleComponents writes an empty CargoHold sized from the module's cargoBay "
    "stats",
    "[module-attach]") {
    entt::registry registry;
    const entt::entity hardpoint = registry.create();
    ModuleDef cargoBay;
    cargoBay.kind = ModuleKind::CargoBay;
    cargoBay.cargoBay.slotCount = 4;
    cargoBay.cargoBay.slotCapacity = 250.0f;

    AttachModuleComponents(registry, hardpoint, cargoBay, 0.0f);

    REQUIRE(registry.all_of<CargoHold>(hardpoint));
    const CargoHold& cargo = registry.get<CargoHold>(hardpoint);
    CHECK(cargo.stacks.empty());
    CHECK(cargo.slotCount == 4);
    CHECK(cargo.slotCapacity == Approx(250.0f));
}

TEST_CASE("DetachModuleComponents removes CargoHold for a cargo-bay module", "[module-attach]") {
    entt::registry registry;
    const entt::entity hardpoint = registry.create();
    ModuleDef cargoBay;
    cargoBay.kind = ModuleKind::CargoBay;
    cargoBay.cargoBay.slotCount = 4;
    cargoBay.cargoBay.slotCapacity = 250.0f;
    AttachModuleComponents(registry, hardpoint, cargoBay, 0.0f);
    REQUIRE(registry.all_of<CargoHold>(hardpoint));

    DetachModuleComponents(registry, hardpoint, cargoBay);

    CHECK_FALSE(registry.all_of<CargoHold>(hardpoint));
}

TEST_CASE(
    "AttachModuleComponents applies FireControl's turnRatePerSecond to a co-mounted Weapon "
    "regardless of attach order",
    "[module-attach]") {
    ModuleDef weapon;
    weapon.kind = ModuleKind::Weapon;
    ModuleDef fireControl;
    fireControl.kind = ModuleKind::FireControl;
    fireControl.fireControl.turnRatePerSecond = 4.0f;

    SECTION("Weapon attaches first") {
        entt::registry registry;
        const entt::entity hardpoint = registry.create();
        AttachModuleComponents(registry, hardpoint, weapon, 0.0f);
        AttachModuleComponents(registry, hardpoint, fireControl, 0.0f);

        REQUIRE(registry.all_of<sr::FiringArc>(hardpoint));
        CHECK(registry.get<sr::FiringArc>(hardpoint).turnRatePerSecond == Approx(4.0f));
    }

    SECTION("FireControl attaches first") {
        entt::registry registry;
        const entt::entity hardpoint = registry.create();
        AttachModuleComponents(registry, hardpoint, fireControl, 0.0f);
        AttachModuleComponents(registry, hardpoint, weapon, 0.0f);

        REQUIRE(registry.all_of<sr::FiringArc>(hardpoint));
        CHECK(registry.get<sr::FiringArc>(hardpoint).turnRatePerSecond == Approx(4.0f));
    }
}

TEST_CASE("A Weapon with no co-mounted FireControl gets the un-augmented kPi traverse baseline",
          "[module-attach]") {
    entt::registry registry;
    const entt::entity hardpoint = registry.create();
    ModuleDef weapon;
    weapon.kind = ModuleKind::Weapon;

    AttachModuleComponents(registry, hardpoint, weapon, 0.0f);

    CHECK(registry.get<sr::FiringArc>(hardpoint).turnRatePerSecond == Approx(sr::kPi));
}

TEST_CASE("DetachModuleComponents removes FireControl and reverts FiringArc to the kPi baseline",
          "[module-attach]") {
    entt::registry registry;
    const entt::entity hardpoint = registry.create();
    ModuleDef weapon;
    weapon.kind = ModuleKind::Weapon;
    ModuleDef fireControl;
    fireControl.kind = ModuleKind::FireControl;
    fireControl.fireControl.turnRatePerSecond = 4.0f;
    AttachModuleComponents(registry, hardpoint, weapon, 0.0f);
    AttachModuleComponents(registry, hardpoint, fireControl, 0.0f);
    REQUIRE(registry.get<sr::FiringArc>(hardpoint).turnRatePerSecond == Approx(4.0f));

    DetachModuleComponents(registry, hardpoint, fireControl);

    CHECK_FALSE(registry.all_of<FireControl>(hardpoint));
    CHECK(registry.get<sr::FiringArc>(hardpoint).turnRatePerSecond == Approx(sr::kPi));
}

TEST_CASE(
    "Sensor, CargoBay, FireControl and Hyperdrive land in the correct power-shedding priority "
    "band",
    "[module-attach]") {
    // architecture.md 12.23: FireControl joins Weapon's band; Sensor/CargoBay/Hyperdrive join
    // Facility's -- no fifth category. Higher sheds later (ModuleAttachment.cpp's SheddingPriority
    // doc comment), so this also orders the four relative to a plain Facility and a Weapon.
    entt::registry registry;

    ModuleDef facility;
    facility.kind = ModuleKind::Facility;
    facility.powerDraw = 1.0f;
    const entt::entity facilityHp = registry.create();
    AttachModuleComponents(registry, facilityHp, facility, 0.0f);

    ModuleDef weapon;
    weapon.kind = ModuleKind::Weapon;
    weapon.powerDraw = 1.0f;
    const entt::entity weaponHp = registry.create();
    AttachModuleComponents(registry, weaponHp, weapon, 0.0f);

    auto priorityOf = [&](ModuleKind kind) {
        ModuleDef module;
        module.kind = kind;
        module.powerDraw = 1.0f;
        const entt::entity hardpoint = registry.create();
        AttachModuleComponents(registry, hardpoint, module, 0.0f);
        return registry.get<sr::PowerLoad>(hardpoint).priority;
    };

    const int facilityPriority = registry.get<sr::PowerLoad>(facilityHp).priority;
    const int weaponPriority = registry.get<sr::PowerLoad>(weaponHp).priority;

    CHECK(priorityOf(ModuleKind::Sensor) == facilityPriority);
    CHECK(priorityOf(ModuleKind::CargoBay) == facilityPriority);
    CHECK(priorityOf(ModuleKind::Hyperdrive) == facilityPriority);
    CHECK(priorityOf(ModuleKind::FireControl) == weaponPriority);
}

TEST_CASE("AttachModuleComponents writes PowerSource/PowerLoad based on the module's stats",
          "[module-attach]") {
    entt::registry registry;
    const entt::entity generator = registry.create();
    ModuleDef cell;
    cell.kind = ModuleKind::PowerCell;
    cell.powerGeneration = 100.0f;
    AttachModuleComponents(registry, generator, cell, 0.0f);
    REQUIRE(registry.all_of<sr::PowerSource>(generator));
    CHECK(registry.get<sr::PowerSource>(generator).generation == 100.0f);

    const entt::entity weapon = registry.create();
    ModuleDef gun;
    gun.kind = ModuleKind::Weapon;
    gun.powerDraw = 20.0f;
    AttachModuleComponents(registry, weapon, gun, 0.0f);
    REQUIRE(registry.all_of<sr::PowerLoad>(weapon));
    CHECK(registry.get<sr::PowerLoad>(weapon).draw == 20.0f);
}

TEST_CASE("DetachModuleComponents removes exactly what AttachModuleComponents added",
          "[module-attach]") {
    entt::registry registry;
    const entt::entity hardpoint = registry.create();
    ModuleDef weapon;
    weapon.kind = ModuleKind::Weapon;
    weapon.powerDraw = 20.0f;
    AttachModuleComponents(registry, hardpoint, weapon, 0.0f);

    DetachModuleComponents(registry, hardpoint, weapon);

    CHECK_FALSE(registry.any_of<sr::Weapon, sr::FiringArc, sr::PowerLoad>(hardpoint));
}

TEST_CASE("DetachModuleComponents removes FacilityRef/DockingBay for a facility module",
          "[module-attach]") {
    entt::registry registry;
    const entt::entity hardpoint = registry.create();
    ModuleDef facility;
    facility.kind = ModuleKind::Facility;
    facility.facility.kind = sr::FacilityKind::Docking;
    AttachModuleComponents(registry, hardpoint, facility, 0.0f);
    REQUIRE(registry.all_of<sr::FacilityRef, sr::DockingBay>(hardpoint));

    DetachModuleComponents(registry, hardpoint, facility);

    CHECK_FALSE(registry.any_of<sr::FacilityRef, sr::DockingBay>(hardpoint));
}

TEST_CASE(
    "DetachModuleComponents subtracts the module's mass back out of HardpointMass and "
    "removes EnginePropulsion",
    "[module-attach]") {
    entt::registry registry;
    const entt::entity hardpoint = registry.create();
    registry.emplace<HardpointMass>(hardpoint, 10.0f);  // Shell mass, survives the detach.
    ModuleDef engine;
    engine.kind = ModuleKind::Engine;
    engine.mass = 6.0f;
    engine.engine.thrustNewtons = 500.0f;
    AttachModuleComponents(registry, hardpoint, engine, 0.0f);
    REQUIRE(registry.get<HardpointMass>(hardpoint).value == Approx(16.0f));

    DetachModuleComponents(registry, hardpoint, engine);

    CHECK(registry.get<HardpointMass>(hardpoint).value == Approx(10.0f));
    CHECK_FALSE(registry.all_of<EnginePropulsion>(hardpoint));
}

TEST_CASE(
    "RecomputeRigTotals sums HardpointMass and EnginePropulsion across living hardpoints, "
    "skipping Destroyed",
    "[module-attach]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<BodyMass>(root);
    registry.emplace<Propulsion>(root);
    registry.emplace<SensorRange>(root);

    // Three engine hardpoints of equal thrust plus one inert armor hardpoint. One engine is
    // already dead -- its mass and thrust must not count.
    const entt::entity engineA = registry.create();
    registry.emplace<HardpointMass>(engineA, 5.0f);
    registry.emplace<EnginePropulsion>(engineA, 100.0f, 10.0f, 300.0f);

    const entt::entity engineB = registry.create();
    registry.emplace<HardpointMass>(engineB, 5.0f);
    registry.emplace<EnginePropulsion>(engineB, 100.0f, 10.0f, 300.0f);

    const entt::entity deadEngine = registry.create();
    registry.emplace<HardpointMass>(deadEngine, 5.0f);
    registry.emplace<EnginePropulsion>(deadEngine, 100.0f, 10.0f, 300.0f);
    registry.emplace<Destroyed>(deadEngine);

    const entt::entity armor = registry.create();
    registry.emplace<HardpointMass>(armor, 20.0f);

    // Two sensor hardpoints of different range -- Max rule, and a dead one that must not count.
    const entt::entity sensorA = registry.create();
    registry.emplace<HardpointSensorRange>(sensorA, 1500.0f);

    const entt::entity sensorB = registry.create();
    registry.emplace<HardpointSensorRange>(sensorB, 2500.0f);

    const entt::entity deadSensor = registry.create();
    registry.emplace<HardpointSensorRange>(deadSensor, 5000.0f);
    registry.emplace<Destroyed>(deadSensor);

    registry.emplace<Rig>(root, std::vector<entt::entity>{engineA, engineB, deadEngine, armor,
                                                          sensorA, sensorB, deadSensor});

    RecomputeRigTotals(registry, root);

    // Mass: Sum rule, dead hardpoints excluded (5 + 5 + 20, not 5 + 5 + 5 + 20).
    CHECK(registry.get<BodyMass>(root).kilograms == Approx(30.0f));
    // Thrust/torque: Sum rule across the two living engines.
    CHECK(registry.get<Propulsion>(root).thrustNewtons == Approx(200.0f));
    CHECK(registry.get<Propulsion>(root).turnTorque == Approx(20.0f));
    // maxSpeed: Max rule, not Sum -- two identical engines do not raise the ceiling.
    CHECK(registry.get<Propulsion>(root).maxSpeed == Approx(300.0f));
    // SensorRange: Max rule, and the dead sensor's higher range must not win.
    CHECK(registry.get<SensorRange>(root).units == Approx(2500.0f));
}

TEST_CASE(
    "RecomputeRigTotals costs thrust proportionally as engines die, zeroing only at the "
    "last",
    "[module-attach]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<BodyMass>(root);
    registry.emplace<Propulsion>(root);

    const entt::entity engineA = registry.create();
    registry.emplace<HardpointMass>(engineA, 5.0f);
    registry.emplace<EnginePropulsion>(engineA, 100.0f, 0.0f, 200.0f);

    const entt::entity engineB = registry.create();
    registry.emplace<HardpointMass>(engineB, 5.0f);
    registry.emplace<EnginePropulsion>(engineB, 100.0f, 0.0f, 200.0f);

    registry.emplace<Rig>(root, std::vector<entt::entity>{engineA, engineB});

    RecomputeRigTotals(registry, root);
    CHECK(registry.get<Propulsion>(root).thrustNewtons == Approx(200.0f));

    registry.emplace<Destroyed>(engineA);
    RecomputeRigTotals(registry, root);
    CHECK(registry.get<Propulsion>(root).thrustNewtons == Approx(100.0f));  // Halved, not zeroed.

    registry.emplace<Destroyed>(engineB);
    RecomputeRigTotals(registry, root);
    CHECK(registry.get<Propulsion>(root).thrustNewtons == Approx(0.0f));  // Zero only now.
}
