#include "modes/space/factories/RigFactory.h"

#include <algorithm>
#include <unordered_map>

#include "shared/blueprints/Validation.h"
#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"

namespace sr::space::rig_factory {
namespace {

// Load-shedding order, consumed by PowerSystem. Higher sheds LAST, so a browning-out rig loses
// facilities first and engines last -- a legible degradation order rather than everything
// failing at once.
int SheddingPriority(ModuleKind kind) {
    switch (kind) {
        case ModuleKind::Facility: return 0;
        case ModuleKind::ShieldGenerator: return 1;
        case ModuleKind::Weapon: return 2;
        case ModuleKind::Engine: return 3;
        default: return 1;
    }
}

// Aggregates accumulated across the whole rig while its hardpoints are built.
struct RigAggregate {
    float mass = 0.0f;
    float extent = 0.0f;
    Propulsion propulsion;
};

// Attaches the role components implied by one module. This is the single place where "what a
// module IS" (authored data) becomes "what a hardpoint DOES" (live components).
void AttachModule(entt::registry& registry, entt::entity hardpoint, const ModuleDef& module,
                  const MountBlueprint& mount, RigAggregate& aggregate) {
    aggregate.mass += module.mass;

    if (module.powerGeneration > 0.0f) {
        registry.emplace_or_replace<PowerSource>(hardpoint, module.powerGeneration);
    }
    if (module.powerDraw > 0.0f) {
        registry.emplace_or_replace<PowerLoad>(hardpoint, module.powerDraw,
                                               SheddingPriority(module.kind));
    }

    switch (module.kind) {
        case ModuleKind::Weapon: {
            const WeaponStats& stats = module.weapon;
            registry.emplace_or_replace<Weapon>(hardpoint, stats.damage, stats.damageType,
                                                stats.fireIntervalSeconds, stats.projectileSpeed,
                                                stats.rangeUnits, stats.spreadRadians,
                                                stats.projectilesPerShot, 0.0f);
            // A zero traverse is a fixed forward mount, which is still a valid arc.
            registry.emplace_or_replace<FiringArc>(hardpoint, mount.traverseRadians, 0.0f, kPi);
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
            // Thrust is a property of the RIG, not of the hardpoint, because acceleration is.
            // PhysicsSystem reads one Propulsion on the root; DamageSystem recomputes it when an
            // engine hardpoint dies. That is what makes "destroy the thruster and it stalls"
            // fall out of the data instead of needing a special case.
            aggregate.propulsion.thrustNewtons += module.engine.thrustNewtons;
            aggregate.propulsion.turnTorque += module.engine.turnTorque;
            aggregate.propulsion.maxSpeed =
                std::max(aggregate.propulsion.maxSpeed, module.engine.maxSpeed);
            break;
        case ModuleKind::Facility:
            // FacilityRef is the generic live-side marker every FacilityKind gets, consumed by
            // BridgeView's tab list (modes/space/ui/, issue #36). DockingBay is DockingSystem's
            // own direct-use tag (issue #24) and stays in addition to it -- Repair/Manufacturing/
            // Research/Storage have no tab CONTENT yet, only the tab existing, until each grows
            // its own system.
            registry.emplace_or_replace<FacilityRef>(hardpoint, module.facility.kind,
                                                     module.facility.level);
            if (module.facility.kind == FacilityKind::Docking) {
                registry.emplace_or_replace<DockingBay>(hardpoint);
            }
            break;
        default: break;
    }
}

entt::entity CreateHardpoint(entt::registry& registry, entt::entity root,
                             const MountBlueprint& mount, const ShellDef& shell,
                             const core::ContentLibrary& content, const WorldTransform& rootXf,
                             RigAggregate& aggregate) {
    const entt::entity hardpoint = registry.create();

    registry.emplace<MountRef>(hardpoint, mount.id);
    registry.emplace<ParentRig>(hardpoint, root);
    registry.emplace<ShellRole>(hardpoint, shell.kind);
    registry.emplace<HitRadius>(hardpoint, shell.radius);
    registry.emplace<LocalTransform>(hardpoint, mount.localOffset, mount.localRotation);

    // Seeded so the first frame renders correctly even before HierarchySystem runs.
    const Vec2 world = rootXf.position + Rotated(mount.localOffset, rootXf.rotation);
    registry.emplace<WorldTransform>(hardpoint, world, rootXf.rotation + mount.localRotation);
    registry.emplace<PreviousTransform>(hardpoint, world, rootXf.rotation + mount.localRotation);

    float hull = shell.hull;
    aggregate.mass += shell.mass;
    aggregate.extent = std::max(aggregate.extent, Length(mount.localOffset) + shell.radius);

    MountedModules mounted;
    for (const ModuleId& moduleId : mount.modules) {
        const ModuleDef* module = content.FindModule(moduleId);
        if (module == nullptr) {
            continue;  // Unreachable: validation resolved every id before we got here.
        }
        hull += module->hullBonus;
        AttachModule(registry, hardpoint, *module, mount, aggregate);
        mounted.ids.push_back(moduleId);
    }
    registry.emplace<MountedModules>(hardpoint, std::move(mounted));

    registry.emplace<Health>(hardpoint, hull, hull);
    return hardpoint;
}

// Second pass: MountBlueprint::attachedTo names a mount by id, which only becomes an entity
// handle once every hardpoint exists.
void ResolveAttachments(entt::registry& registry, const RigBlueprint& rig,
                        const std::unordered_map<std::string, entt::entity>& byMount) {
    for (const MountBlueprint& mount : rig.mounts) {
        const auto self = byMount.find(mount.id.str());
        if (self == byMount.end()) {
            continue;
        }
        entt::entity parent = entt::null;
        if (!mount.attachedTo.empty()) {
            const auto found = byMount.find(mount.attachedTo.str());
            parent = found == byMount.end() ? entt::null : found->second;
        }
        registry.emplace<StructuralAttachment>(self->second, parent);
    }
}

}  // namespace

SpawnResult Spawn(SystemWorld& world, const core::ContentLibrary& content,
                  const SpawnParams& params) {
    const ShipBlueprint* blueprint = content.FindShip(params.blueprint);
    if (blueprint == nullptr || !Validate(*blueprint, content).ok()) {
        return {};
    }

    entt::registry& registry = world.Registry();
    const entt::entity root = registry.create();

    const WorldTransform rootXf{params.position, params.rotation};
    registry.emplace<WorldTransform>(root, rootXf);
    registry.emplace<PreviousTransform>(root, rootXf.position, rootXf.rotation);
    registry.emplace<BlueprintRef>(root, blueprint->id);
    registry.emplace<FactionRef>(root,
                                 params.faction.empty() ? blueprint->faction : params.faction);
    registry.emplace<DisplayName>(root, blueprint->displayName);

    RigAggregate aggregate;
    std::unordered_map<std::string, entt::entity> byMount;
    Rig rig;
    rig.children.reserve(blueprint->rig.mounts.size());

    for (const MountBlueprint& mount : blueprint->rig.mounts) {
        const ShellDef* shell = content.FindShell(mount.shell);
        if (shell == nullptr) {
            continue;
        }
        const entt::entity hardpoint =
            CreateHardpoint(registry, root, mount, *shell, content, rootXf, aggregate);
        byMount.emplace(mount.id.str(), hardpoint);
        rig.children.push_back(hardpoint);
    }

    ResolveAttachments(registry, blueprint->rig, byMount);
    registry.emplace<Rig>(root, std::move(rig));

    registry.emplace<BodyMass>(root, std::max(aggregate.mass, 1.0f));
    registry.emplace<CollisionRadius>(root, std::max(aggregate.extent, 1.0f));
    registry.emplace<RamCooldown>(root);
    registry.emplace<Velocity>(root);
    registry.emplace<ThrustInput>(root);
    registry.emplace<PowerBudget>(root);
    registry.emplace<Target>(root);
    registry.emplace<Targetable>(root);
    registry.emplace<SensorRange>(root, 2000.0f);

    // Stations get no Propulsion at all, rather than a zeroed one. PhysicsSystem's view then
    // excludes them structurally instead of by a branch on a `mobile` flag.
    if (blueprint->mobile) {
        registry.emplace<Propulsion>(root, aggregate.propulsion);
        registry.emplace<LinearDamping>(root, 0.35f, 2.5f);
    }

    return {root, world.Track(root)};
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

}  // namespace sr::space::rig_factory
