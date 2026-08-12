#include "shared/rig/ModuleAttachment.h"

#include <algorithm>

#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/math/Angle.h"

namespace sr::rig_attachment {
namespace {

// Load-shedding order, consumed by PowerSystem. Higher sheds LAST, so a browning-out rig loses
// facilities first and engines last -- moved here verbatim from RigFactory.cpp's identical
// helper, since both attach paths need the same order.
int SheddingPriority(ModuleKind kind) {
    switch (kind) {
        case ModuleKind::Facility: return 0;
        case ModuleKind::ShieldGenerator: return 1;
        case ModuleKind::Weapon: return 2;
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
            registry.emplace_or_replace<FiringArc>(hardpoint, mountTraverseRadians, 0.0f, kPi);
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
        default: break;
    }
}

void RecomputeRigTotals(entt::registry& registry, entt::entity rigRoot) {
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return;
    }

    float mass = 0.0f;
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
    }

    if (auto* bodyMass = registry.try_get<BodyMass>(rigRoot)) {
        bodyMass->kilograms = std::max(mass, 1.0f);
    }
    if (auto* rigPropulsion = registry.try_get<Propulsion>(rigRoot)) {
        *rigPropulsion = propulsion;
    }
}

}  // namespace sr::rig_attachment
