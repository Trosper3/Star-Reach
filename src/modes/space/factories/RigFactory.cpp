#include "modes/space/factories/RigFactory.h"

#include <algorithm>
#include <unordered_map>

#include "shared/blueprints/Validation.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Power.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/rig/ModuleAttachment.h"

namespace sr::space::rig_factory {
namespace {

// Aggregates accumulated across the whole rig while its hardpoints are built.
struct RigAggregate {
    float mass = 0.0f;
    float extent = 0.0f;
    Propulsion propulsion;
};

// Folds one module's mass and (if any) Propulsion contribution into `aggregate`, after
// shared/rig/ModuleAttachment.h's AttachModuleComponents has already written the module's role
// components (Weapon/Shield/PowerSource/FacilityRef) -- the same "what a module IS becomes what
// a hardpoint DOES" step architecture.md 12.11 also needs for live equip, extracted there so
// modes/space/systems/ never has to include factories/ (section 2.3) to reuse it.
void AttachModule(entt::registry& registry, entt::entity hardpoint, const ModuleDef& module,
                  const MountBlueprint& mount, RigAggregate& aggregate) {
    aggregate.mass += module.mass;

    const rig_attachment::PropulsionContribution propulsion =
        rig_attachment::AttachModuleComponents(registry, hardpoint, module, mount.traverseRadians);
    if (propulsion.present) {
        // Thrust is a property of the RIG, not of the hardpoint, because acceleration is.
        // PhysicsSystem reads one Propulsion on the root; DamageSystem recomputes it when an
        // engine hardpoint dies. That is what makes "destroy the thruster and it stalls" fall
        // out of the data instead of needing a special case.
        aggregate.propulsion.thrustNewtons += propulsion.thrustNewtons;
        aggregate.propulsion.turnTorque += propulsion.turnTorque;
        aggregate.propulsion.maxSpeed =
            std::max(aggregate.propulsion.maxSpeed, propulsion.maxSpeed);
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
    registry.emplace<MountTraverse>(hardpoint, mount.traverseRadians);
    registry.emplace<LocalTransform>(hardpoint, mount.localOffset, mount.localRotation);

    // Seeded so the first frame renders correctly even before HierarchySystem runs.
    const Vec2 world = rootXf.position + Rotated(mount.localOffset, rootXf.rotation);
    registry.emplace<WorldTransform>(hardpoint, world, rootXf.rotation + mount.localRotation);
    registry.emplace<PreviousTransform>(hardpoint, world, rootXf.rotation + mount.localRotation);

    float hull = shell.hull;
    aggregate.mass += shell.mass;
    aggregate.extent = std::max(aggregate.extent, Length(mount.localOffset) + shell.radius);

    // Seeded with the shell's own mass so AttachModule's HardpointMass::get_or_emplace (via
    // AttachModuleComponents) adds each mounted module's mass on top of it, not in place of it --
    // the same split RecomputeRigTotals later reads back for a live refit or a hardpoint's death.
    registry.emplace<HardpointMass>(hardpoint, shell.mass);

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

    // Always emplaced, on every root -- a rig moves because it has living engines, not because a
    // blueprint flag says it may (architecture.md 12.25). A station's aggregate.propulsion stays
    // zero (nothing in its mount list contributed to it), and PhysicsSystem already treats zero
    // thrust as "does not move" -- so exclusion becomes numerical, the same answer every other
    // rig-level attribute gives, instead of a vessel-type branch (Law 4). LinearDamping applying
    // to every root is what stops a station inside kSunGravityRange accelerating without bound
    // (architecture.md 13.3 finding J): before this, only a mobile rig had anything to bleed off
    // the velocity OrbitSystem's gravity loop adds every tick.
    //
    // `mobile` survives with a narrower job: StationFactory::Spawn still rejects `mobile: true`,
    // and Validation still requires a mobile blueprint to author at least one engine shell. It no
    // longer decides whether physics applies.
    registry.emplace<Propulsion>(root, aggregate.propulsion);
    registry.emplace<LinearDamping>(root, 0.35f, 2.5f);

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
