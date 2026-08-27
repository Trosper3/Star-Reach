#include "shared/rig/ModuleAttachment.h"

#include <algorithm>

#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Physics.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"
#include "shared/math/Vec2.h"

namespace sr::rig_attachment {
namespace {

// Which of the four player-commandable power categories a module's draw belongs to (features.md
// section 2.9). architecture.md 12.23: FireControl joins Weapon's band (part of the gunnery
// chain); Sensor/CargoBay/Hyperdrive join Facility's (non-combat) -- no fifth category for the
// four new kinds, the same grouping this used to encode as a bare shed-order int before
// PowerPriorityList (Power.h) made shed order player-configurable instead of fixed by kind.
PowerCategory PowerCategoryFor(ModuleKind kind) {
    switch (kind) {
        case ModuleKind::Facility:
        case ModuleKind::Sensor:
        case ModuleKind::CargoBay:
        case ModuleKind::Hyperdrive: return PowerCategory::Facilities;
        case ModuleKind::ShieldGenerator: return PowerCategory::Shields;
        case ModuleKind::Weapon:
        case ModuleKind::FireControl: return PowerCategory::Weapons;
        case ModuleKind::Engine: return PowerCategory::Engines;
        default: return PowerCategory::Facilities;
    }
}

// Attaches the role components specific to `module.kind` -- everything AttachModuleComponents
// itself does not already handle uniformly for every kind (HardpointMass, PowerSource/PowerLoad).
// Split out purely to keep AttachModuleComponents under architecture.md 2.2's function-length cap;
// no independent meaning outside that caller.
PropulsionContribution AttachRoleComponents(entt::registry& registry, entt::entity hardpoint,
                                            const ModuleDef& module, float mountTraverseRadians) {
    PropulsionContribution propulsion;
    switch (module.kind) {
        case ModuleKind::Weapon: {
            const WeaponStats& stats = module.weapon;
            registry.emplace_or_replace<Weapon>(hardpoint, stats.damage, stats.damageType,
                                                stats.fireIntervalSeconds, stats.projectileSpeed,
                                                stats.rangeUnits, stats.spreadRadians,
                                                stats.projectilesPerShot, 0.0f);
            // A FireControl module may already be mounted here (mount.modules' order isn't
            // guaranteed), in which case its rate applies immediately instead of the kPi baseline.
            const auto* fireControl = registry.try_get<FireControl>(hardpoint);
            const float turnRate = fireControl != nullptr ? fireControl->turnRatePerSecond : kPi;
            registry.emplace_or_replace<FiringArc>(hardpoint, mountTraverseRadians, 0.0f, turnRate);
            break;
        }
        case ModuleKind::ShieldGenerator: {
            const ShieldStats& stats = module.shield;
            registry.emplace_or_replace<Shield>(
                hardpoint, stats.capacity, stats.capacity, stats.absorbs, stats.rechargePerSecond,
                stats.rechargeDelaySeconds, 0.0f, stats.coverage, stats.coverageRadius);
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
            registry.emplace_or_replace<FacilityRef>(
                hardpoint, module.facility.kind, module.facility.grade, module.facility.capacity);
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
        case ModuleKind::Crew:
            // Cached, not returned -- RecomputeRigTotals reads this back later across every
            // living hardpoint, the same shape HardpointSensorRange already uses.
            registry.emplace_or_replace<CrewRating>(hardpoint, module.crew.operation,
                                                    module.crew.command, module.crew.sensors,
                                                    module.crew.repair);
            break;
        default: break;
    }
    return propulsion;
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
        registry.emplace_or_replace<PowerLoad>(hardpoint, module.powerDraw, module.powerLevels,
                                               PowerCategoryFor(module.kind));
    }

    return AttachRoleComponents(registry, hardpoint, module, mountTraverseRadians);
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
        case ModuleKind::Crew: registry.remove<CrewRating>(hardpoint); break;
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

    // features.md 2.7: a turret's own Crew (ShellRole::kind == Weapon) is scoped to that
    // hardpoint alone -- independent tracking is TargetingSystem/WeaponSystem's concern, not the
    // rig's. Every other crew-carrying shell is the rig's control shell (a cockpit or bridge) and
    // is what keeps the whole rig flying and firing; `crewed` tracks whether ANY of those is still
    // alive, and the two bonuses below are Maxed across them the same way a rig never benefits
    // twice from two of the same specialist.
    bool crewed = false;
    float sensorCrewBonus = 0.0f;
    float repairCrewBonus = 0.0f;

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
        if (const auto* crew = registry.try_get<CrewRating>(hardpoint)) {
            const auto* role = registry.try_get<ShellRole>(hardpoint);
            if (role == nullptr || role->kind != ShellKind::Weapon) {
                crewed = true;
                sensorCrewBonus = std::max(sensorCrewBonus, crew->sensors);
                repairCrewBonus = std::max(repairCrewBonus, crew->repair);
            }
        }
    }

    if (auto* bodyMass = registry.try_get<BodyMass>(rigRoot)) {
        bodyMass->kilograms = std::max(mass, 1.0f);
    }
    if (auto* rigPropulsion = registry.try_get<Propulsion>(rigRoot)) {
        *rigPropulsion = propulsion;
    }
    if (auto* rigSensor = registry.try_get<SensorRange>(rigRoot)) {
        // features.md 2.7's third aggregation rule, beside Sum and Max above: an officer
        // multiplies a base stat rather than contributing a term inside the pass that built it, so
        // the crew bonus is applied only after sensorRange itself is already final.
        rigSensor->units = sensorRange * (1.0f + sensorCrewBonus);
    }
    if (repairCrewBonus > 0.0f) {
        registry.emplace_or_replace<CrewRepairBonus>(rigRoot, repairCrewBonus);
    } else {
        registry.remove<CrewRepairBonus>(rigRoot);
    }

    // features.md 3.2's uncrewed hull: no living control-shell crew means the rig stops flying and
    // firing (NpcAiSystem's crew check below) without being destroyed -- it keeps its velocity and
    // drifts, remains targetable and collidable, and can be captured.
    if (crewed) {
        registry.remove<Uncrewed>(rigRoot);
    } else {
        registry.emplace_or_replace<Uncrewed>(rigRoot);
    }
}

float AggregateStructuralIntegrity(const entt::registry& registry, entt::entity rigRoot) {
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr || rig->children.empty()) {
        return 0.0f;
    }

    float current = 0.0f;
    float max = 0.0f;
    for (const entt::entity child : rig->children) {
        if (const Health* health = registry.try_get<Health>(child)) {
            max += health->max;
            if (!registry.all_of<Destroyed>(child)) {
                current += health->current;
            }
        }
    }
    return max > 0.0f ? current / max : 0.0f;
}

bool ShieldCovers(const entt::registry& registry, entt::entity shieldEntity,
                  entt::entity hardpoint) {
    if (shieldEntity == hardpoint) {
        return true;
    }
    const auto* shield = registry.try_get<Shield>(shieldEntity);
    if (shield == nullptr) {
        return false;
    }
    if (shield->coverage == ShieldCoverage::Conformal) {
        return true;
    }
    if (shield->coverage == ShieldCoverage::Personal) {
        return false;
    }
    const auto* shieldXf = registry.try_get<WorldTransform>(shieldEntity);
    const auto* targetXf = registry.try_get<WorldTransform>(hardpoint);
    if (shieldXf == nullptr || targetXf == nullptr) {
        return false;
    }
    return Distance(shieldXf->position, targetXf->position) <= shield->coverageRadius;
}

entt::entity FindHardpoint(const entt::registry& registry, entt::entity root,
                           const MountId& mount) {
    const Rig* rig = registry.try_get<Rig>(root);
    if (rig == nullptr) {
        return entt::null;
    }
    for (const entt::entity child : rig->children) {
        const MountRef* ref = registry.try_get<MountRef>(child);
        if (ref != nullptr && ref->id == mount) {
            return child;
        }
    }
    return entt::null;
}

entt::entity CreateBareHardpoint(entt::registry& registry, entt::entity root,
                                 const MountBlueprint& mount, const ShellDef& shell) {
    const entt::entity hardpoint = registry.create();

    registry.emplace<MountRef>(hardpoint, mount.id);
    registry.emplace<ParentRig>(hardpoint, root);
    registry.emplace<ShellRole>(hardpoint, shell.kind, shell.acceptsKinds);
    registry.emplace<HitRadius>(hardpoint, shell.radius);
    registry.emplace<MountTraverse>(hardpoint, mount.traverseRadians);
    registry.emplace<LocalTransform>(hardpoint, mount.localOffset, mount.localRotation);
    registry.emplace<DrawLayer>(
        hardpoint, mount.drawLayerOverride != 0 ? mount.drawLayerOverride : shell.drawLayer);

    const WorldTransform& rootXf = registry.get<WorldTransform>(root);
    const Vec2 world = rootXf.position + Rotated(mount.localOffset, rootXf.rotation);
    registry.emplace<WorldTransform>(hardpoint, world, rootXf.rotation + mount.localRotation);
    registry.emplace<PreviousTransform>(hardpoint, world, rootXf.rotation + mount.localRotation);

    registry.emplace<HardpointMass>(hardpoint, shell.mass);
    registry.emplace<MountedModules>(hardpoint);
    registry.emplace<Health>(hardpoint, shell.hull, shell.hull);
    return hardpoint;
}

}  // namespace sr::rig_attachment
