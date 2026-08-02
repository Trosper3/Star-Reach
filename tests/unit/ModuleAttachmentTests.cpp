#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Power.h"
#include "shared/rig/ModuleAttachment.h"

using sr::ModuleDef;
using sr::ModuleKind;
using sr::rig_attachment::AttachModuleComponents;
using sr::rig_attachment::DetachModuleComponents;

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
    // Engine contribution is reported, not written directly -- no rig-wide state to write to
    // from a single hardpoint (this header's comment).
    CHECK_FALSE(registry.any_of<sr::Weapon, sr::Shield, sr::FacilityRef>(hardpoint));
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

    DetachModuleComponents(registry, hardpoint, ModuleKind::Weapon);

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

    DetachModuleComponents(registry, hardpoint, ModuleKind::Facility);

    CHECK_FALSE(registry.any_of<sr::FacilityRef, sr::DockingBay>(hardpoint));
}
