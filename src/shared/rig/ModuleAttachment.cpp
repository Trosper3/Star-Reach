#include "shared/rig/ModuleAttachment.h"

#include <algorithm>

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

namespace sr::rig_attachment {
namespace {

// Load-shedding order, consumed by PowerSystem. Higher sheds LAST, so a browning-out rig loses
// facilities first and engines last -- moved here verbatim from RigFactory.cpp's identical
// helper, since both attach paths need the same order. architecture.md 12.23: FireControl joins
// Weapon's band (part of the gunnery chain); Sensor/CargoBay/Hyperdrive join Facility's (shed
// first, non-combat) -- no fifth category for the four new kinds.
int SheddingPriority(ModuleKind kind) {
    switch (kind) {
        case ModuleKind::Facility:
        case ModuleKind::Sensor:
        case ModuleKind::CargoBay:
        case ModuleKind::Hyperdrive: return 0;
        case ModuleKind::ShieldGenerator: return 1;
        case ModuleKind::Weapon:
        case ModuleKind::FireControl: return 2;
        case ModuleKind::Engine: return 3;
        default: return 1;
    }
}

}  // namespace

PropulsionContribution AttachModuleComponents(entt::registry& registry, entt::entity hardpoint,
                                              const ModuleDef& module, float mountTraverseRadians) {
    // get_or_emplace, not emplace: RigFactory seeds HardpointMass with the shell's own mass
    // before this runs, and this call needs to add on top of that, not replace it. A hardpoint
    // that never got seeded (a live-equip test built by hand, or the shellless bare-registry
    // unit tests in ModuleAttachmentTests.cpp) simply starts from zero.
    registry.get_or_emplace<HardpointMass>(hardpoint).value += module.mass;

    if (module.powerGeneration > 0.0f) {
        registry.emplace_or_replace<PowerSource>(hardpoint, module.powerGeneration);
    }
    if (module.powerDraw > 0.0f) {
        registry.emplace_or_replace<PowerLoad>(hardpoint, module.powerDraw,
                                               SheddingPriority(module.kind));
    }

    PropulsionContribution propulsion;
    switch (module.kind) {
        case ModuleKind::Weapon: {
            const WeaponStats& stats = module.weapon;
            registry.emplace_or_replace<Weapon>(hardpoint, stats.damage, stats.damageType,
                                                stats.fireIntervalSeconds, stats.projectileSpeed,
                                                stats.rangeUnits, stats.spreadRadians,
                                                stats.projectilesPerShot, 0.0f);
            // A FireControl module may already be mounted on this hardpoint -- mount.modules'
            // authored order is not guaranteed -- in which case its rate applies immediately
            // instead of the kPi un-augmented baseline (a manually slaved turret's traverse).
            const auto* fireControl = registry.try_get<FireControl>(hardpoint);
            const float turnRate = fireControl != nullptr ? fireControl->turnRatePerSecond : kPi;
            registry.emplace_or_replace<FiringArc>(hardpoint, mountTraverseRadians, 0.0f, turnRate);
            break;
        }
        case ModuleKind::ShieldGenerator: {
            const ShieldStats& stats = module.shield;
            registry.emplace_or_replace<Shield>(hardpoint, stats.capacity, stats.capacity,
                                                stats.absorbs, stats.rechargePerSecond,
                                                stats.rechargeDelaySeconds, 0.0f);
            break;
        }
        case ModuleKind::Engine:
            propulsion.present = true;
            propulsion.thrustNewtons = module.engine.thrustNewtons;
            propulsion.turnTorque = module.engine.turnTorque;
            propulsion.maxSpeed = module.engine.maxSpeed;
            // Cached on the hardpoint itself so RecomputeRigTotals can fold it into the rig's
            // Propulsion later without needing the ModuleDef (and therefore ContentLibrary)
            // again -- the reported PropulsionContribution below is what RigFactory's own
            // fresh-build aggregation still uses.
            registry.emplace_or_replace<EnginePropulsion>(
                hardpoint, propulsion.thrustNewtons, propulsion.turnTorque, propulsion.maxSpeed);
            break;
        case ModuleKind::Facility:
            registry.emplace_or_replace<FacilityRef>(hardpoint, module.facility.kind);
            if (module.facility.kind == FacilityKind::Docking) {
                registry.emplace_or_replace<DockingBay>(hardpoint);
            }
            break;
        case ModuleKind::Sensor:
            // Cached, not returned -- RecomputeRigTotals reads this back later without needing
            // the ModuleDef again, the same shape as EnginePropulsion above.
            registry.emplace_or_replace<HardpointSensorRange>(hardpoint, module.sensor.range);
            break;
        case ModuleKind::CargoBay:
            // slotCount/slotCapacity copied from the module instance, the same pattern
            // ModuleKind::Facility uses for FacilityRef::kind above. Always starts empty --
            // re-attaching onto an already-loaded bay without detaching first is not a supported
            // path (RigFactory only ever attaches once; ModuleEquipSystem detaches before it
            // would attach again).
            registry.emplace_or_replace<CargoHold>(hardpoint, std::vector<ItemStack>{},
                                                   module.cargoBay.slotCount,
                                                   module.cargoBay.slotCapacity);
            break;
        case ModuleKind::FireControl:
            registry.emplace_or_replace<FireControl>(hardpoint,
                                                     module.fireControl.turnRatePerSecond);
            // The reverse order: a Weapon already sitting on this hardpoint gets its FiringArc
            // updated directly, since that Weapon's own AttachModuleComponents call already ran
            // and cannot be retriggered.
            if (auto* arc = registry.try_get<FiringArc>(hardpoint)) {
                arc->turnRatePerSecond = module.fireControl.turnRatePerSecond;
            }
            break;
        default: break;
    }
    return propulsion;
}

void DetachModuleComponents(entt::registry& registry, entt::entity hardpoint,
                            const ModuleDef& module) {
    if (auto* mass = registry.try_get<HardpointMass>(hardpoint)) {
        mass->value = std::max(0.0f, mass->value - module.mass);
    }

    registry.remove<PowerSource>(hardpoint);
    registry.remove<PowerLoad>(hardpoint);

    switch (module.kind) {
        case ModuleKind::Weapon:
            registry.remove<Weapon>(hardpoint);
            registry.remove<FiringArc>(hardpoint);
            break;
        case ModuleKind::ShieldGenerator: registry.remove<Shield>(hardpoint); break;
        case ModuleKind::Engine: registry.remove<EnginePropulsion>(hardpoint); break;
        case ModuleKind::Facility:
            registry.remove<FacilityRef>(hardpoint);
            registry.remove<DockingBay>(hardpoint);
            break;
        case ModuleKind::Sensor: registry.remove<HardpointSensorRange>(hardpoint); break;
        // Whatever the bay held is the caller's problem to spill first (LootSystem's
        // SpillCargoHold, called by ModuleEquipSystem's unmount path before this runs) -- this
        // function only ever tears down role components, never spawns entities.
        case ModuleKind::CargoBay: registry.remove<CargoHold>(hardpoint); break;
        case ModuleKind::FireControl:
            registry.remove<FireControl>(hardpoint);
            // Revert to the un-augmented baseline rather than leaving the co-mounted Weapon's
            // FiringArc at a rate nothing authored anymore.
            if (auto* arc = registry.try_get<FiringArc>(hardpoint)) {
                arc->turnRatePerSecond = kPi;
            }
            break;
        default: break;
    }
}

void RecomputeRigTotals(entt::registry& registry, entt::entity rigRoot) {
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return;
    }

    float mass = 0.0f;
    float sensorRange = 0.0f;
    Propulsion propulsion;
    for (const entt::entity hardpoint : rig->children) {
        if (registry.all_of<Destroyed>(hardpoint)) {
            continue;
        }
        if (const auto* hardpointMass = registry.try_get<HardpointMass>(hardpoint)) {
            mass += hardpointMass->value;
        }
        if (const auto* engine = registry.try_get<EnginePropulsion>(hardpoint)) {
            propulsion.thrustNewtons += engine->thrustNewtons;
            propulsion.turnTorque += engine->turnTorque;
            propulsion.maxSpeed = std::max(propulsion.maxSpeed, engine->maxSpeed);
        }
        if (const auto* sensor = registry.try_get<HardpointSensorRange>(hardpoint)) {
            sensorRange = std::max(sensorRange, sensor->value);
        }
    }

    if (auto* bodyMass = registry.try_get<BodyMass>(rigRoot)) {
        bodyMass->kilograms = std::max(mass, 1.0f);
    }
    if (auto* rigPropulsion = registry.try_get<Propulsion>(rigRoot)) {
        *rigPropulsion = propulsion;
    }
    if (auto* rigSensor = registry.try_get<SensorRange>(rigRoot)) {
        rigSensor->units = sensorRange;
    }
}

}  // namespace sr::rig_attachment
