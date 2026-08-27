// EngineeringScreen.cpp's own pure data half, split out to satisfy architecture.md 2.2's 600-line
// file cap (issue #230's schematic-plus-chrome pass pushed the drawing half well past it on its
// own) -- the same split StorageScreen.cpp/StorageScreenModel.cpp, RepairScreen.cpp/
// RepairScreenModel.cpp and ModulesMenu.cpp/ModulesMenuModel.cpp already establish. Everything
// here carries no raylib and no layout, so it costs nothing to keep in its own translation unit,
// unlike Update()/Draw()'s input polling and layout math, which stay with the widgets they
// position.
#include "modes/space/ui/EngineeringScreen.h"

#include <algorithm>
#include <string>

#include "core/registries/ContentLibrary.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/rig/CargoView.h"

namespace sr::space::ui::engineering_screen {
namespace {

// One-letter monogram per ShellKind (features.md 3.9's glyph-carries-identity rule) -- the same
// placeholder every other docked screen's own row list uses; duplicated locally rather than
// shared, since none of them may depend on each other and this is the whole of it.
void ShellGlyph(ShellKind kind, char (&out)[3]) {
    switch (kind) {
        case ShellKind::Chassis: out[0] = 'C'; break;
        case ShellKind::Armor: out[0] = 'A'; break;
        case ShellKind::PowerCell: out[0] = 'P'; break;
        case ShellKind::Engine: out[0] = 'E'; break;
        case ShellKind::Weapon: out[0] = 'W'; break;
        case ShellKind::Shield: out[0] = 'S'; break;
        case ShellKind::Facility: out[0] = 'F'; break;
    }
    out[1] = '\0';
    out[2] = '\0';
}

bool HasDependentChild(const entt::registry& registry, entt::entity rigRoot,
                       entt::entity hardpoint) {
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return false;
    }
    for (const entt::entity child : rig->children) {
        if (child == hardpoint) {
            continue;
        }
        const StructuralAttachment* attachment = registry.try_get<StructuralAttachment>(child);
        if (attachment != nullptr && attachment->attachedTo == hardpoint) {
            return true;
        }
    }
    return false;
}

// The hardpoint in `rigRoot`'s Rig carrying MountRef::id == `mount`, or entt::null. Whether it is
// living or Destroyed is the caller's concern -- this only answers "is there an entity at all."
entt::entity FindMount(const entt::registry& registry, entt::entity rigRoot, const MountId& mount) {
    const Rig* rig = registry.try_get<Rig>(rigRoot);
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

}  // namespace

entt::entity PlayerShell(const entt::registry& registry) {
    for (const entt::entity entity : registry.view<PlayerLocation>()) {
        return entity;
    }
    return entt::null;
}

entt::entity CurrentFacility(const entt::registry& registry, entt::entity shell) {
    if (shell == entt::null || registry.all_of<Destroyed>(shell)) {
        return entt::null;
    }
    const FacilityRef* facility = registry.try_get<FacilityRef>(shell);
    if (facility == nullptr || facility->kind != FacilityKind::Engineering) {
        return entt::null;
    }
    // architecture.md 12.30's frame: at most one full-screen tab at a time, and Storage's own
    // selection (bridge_view::IsStorageSelected) has no hardpoint to contest this gate with
    // otherwise.
    if (bridge_view::IsStorageSelected(registry)) {
        return entt::null;
    }
    return shell;
}

std::vector<entt::entity> SiblingBenches(const entt::registry& registry, entt::entity station) {
    std::vector<entt::entity> benches;
    const Rig* rig = registry.try_get<Rig>(station);
    if (rig == nullptr) {
        return benches;
    }
    for (const entt::entity child : rig->children) {
        if (registry.all_of<Destroyed>(child)) {
            continue;
        }
        const FacilityRef* facility = registry.try_get<FacilityRef>(child);
        if (facility != nullptr && facility->kind == FacilityKind::Engineering) {
            benches.push_back(child);
        }
    }
    return benches;
}

std::vector<ModuleRow> ModuleRows(const entt::registry& registry, entt::entity requester) {
    std::vector<ModuleRow> rows;
    for (const ItemStack& stack : cargo_view::Merged(registry, requester)) {
        if (stack.kind != ItemKind::Module) {
            continue;
        }
        ModuleRow entry;
        entry.module = ModuleId(stack.id);
        entry.row.label = stack.id;
        entry.row.value = "DRAG TO MOVE";  // Overwritten by DrawCargoRows anyway; kept accurate
                                           // for anything reading Row.value straight off this.
        rows.push_back(std::move(entry));
    }
    return rows;
}

std::vector<entt::entity> DeletableHardpoints(const entt::registry& registry,
                                              entt::entity rigRoot) {
    std::vector<entt::entity> deletable;
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return deletable;
    }
    for (const entt::entity hardpoint : rig->children) {
        if (!HasDependentChild(registry, rigRoot, hardpoint)) {
            deletable.push_back(hardpoint);
        }
    }
    return deletable;
}

std::vector<MountId> RebuildableMounts(const entt::registry& registry, entt::entity rigRoot,
                                       const RigBlueprint& blueprint) {
    std::vector<MountId> missing;
    for (const MountBlueprint& mount : blueprint.mounts) {
        if (FindMount(registry, rigRoot, mount.id) == entt::null) {
            missing.push_back(mount.id);
        }
    }
    return missing;
}

std::vector<MountRow> MountRows(const entt::registry& registry, entt::entity rigRoot,
                                const RigBlueprint& blueprint) {
    std::vector<MountRow> rows;
    const std::vector<entt::entity> deletable = DeletableHardpoints(registry, rigRoot);
    const std::vector<MountId> rebuildable = RebuildableMounts(registry, rigRoot, blueprint);
    rows.reserve(blueprint.mounts.size());

    for (const MountBlueprint& mount : blueprint.mounts) {
        MountRow entry;
        entry.mount = mount.id;
        entry.row.label = mount.id.str();

        const bool missing =
            std::find(rebuildable.begin(), rebuildable.end(), mount.id) != rebuildable.end();
        if (missing) {
            // "Outline only, MISSING" (architecture.md 12.30.5) -- greyed only when its authored
            // attachedTo parent is itself missing or Destroyed, the orphan refusal RefactorSystem
            // applies at request time, mirrored here so the click never has a reason to fail.
            bool parentReady = mount.attachedTo.empty();
            if (!parentReady) {
                const entt::entity parent = FindMount(registry, rigRoot, mount.attachedTo);
                parentReady = parent != entt::null && !registry.all_of<Destroyed>(parent);
            }
            entry.row.style.integrity = 0.0f;
            entry.row.style.disabled = !parentReady;
            entry.row.value = "REBUILD";
        } else {
            entry.hardpoint = FindMount(registry, rigRoot, mount.id);
            entry.destroyed = registry.all_of<Destroyed>(entry.hardpoint);
            entry.deletable =
                std::find(deletable.begin(), deletable.end(), entry.hardpoint) != deletable.end();
            entry.holdsModules =
                !entry.destroyed && HardpointHoldsModules(registry, entry.hardpoint);
            if (const ShellRole* role = registry.try_get<ShellRole>(entry.hardpoint)) {
                ShellGlyph(role->kind, entry.row.glyph);
            }
            if (const Health* health = registry.try_get<Health>(entry.hardpoint)) {
                entry.row.style.integrity =
                    health->max > 0.0f ? health->current / health->max : 0.0f;
            }
            if (entry.destroyed) {
                entry.row.style.disabled = true;
                entry.row.value = "DESTROYED";
            } else if (entry.holdsModules) {
                // architecture.md 15.2 finding 8: RefactorSystem has always silently refused to
                // delete a hardpoint that still holds a module -- "unmount first, then delete."
                // Surfacing that here is what fixes the "click Delete, nothing
                // happens" bug: the row explains the refusal instead of pretending Delete would
                // work. Deliberately NOT `style.disabled = true`: that would grey the schematic
                // node instead of colouring it by integrity (features.md 3.9 -- colour carries
                // condition, independent of which verb happens to be available), and since almost
                // every hardpoint on a freshly spawned ship holds a module, that greyed out
                // basically the entire schematic, hiding both INSTALLED and DAMAGED. Update()'s
                // BeginGesture already routes a holdsModules row into an Unmount drag before it
                // would ever reach TryClickNode's own disabled check, so nothing here needs
                // `style.disabled` to also mean "a click would do nothing."
                entry.row.style.disabled = false;
                entry.row.value = "UNMOUNT FIRST";
            } else {
                entry.row.style.disabled = !entry.deletable;
                entry.row.value = "DELETE";
            }
        }
        rows.push_back(std::move(entry));
    }
    return rows;
}

entt::entity OwnedVesselAt(const entt::registry& registry, entt::entity station,
                           const FactionId& playerFaction) {
    for (auto [vessel, docked, faction] : registry.view<Docked, FactionRef>().each()) {
        if (docked.station == station && faction.id == playerFaction) {
            return vessel;
        }
    }
    return entt::null;
}

bool StationIsSubject(const entt::registry& registry, entt::entity station,
                      const FactionId& playerFaction) {
    const FactionRef* stationFaction = registry.try_get<FactionRef>(station);
    return stationFaction != nullptr && stationFaction->id == playerFaction;
}

std::string FormattedMountLabel(const std::string& id) {
    std::string label = id;
    for (char& c : label) {
        if (c == '_') {
            c = ' ';
        } else if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
    }
    return label;
}

bool HardpointHoldsModules(const entt::registry& registry, entt::entity hardpoint) {
    const MountedModules* mounted = registry.try_get<MountedModules>(hardpoint);
    return mounted != nullptr && !mounted->ids.empty();
}

std::string MountedModuleLabel(const entt::registry& registry, const core::ContentLibrary& content,
                               entt::entity hardpoint) {
    if (hardpoint == entt::null || registry.all_of<Destroyed>(hardpoint)) {
        return {};
    }
    const MountedModules* mounted = registry.try_get<MountedModules>(hardpoint);
    if (mounted == nullptr || mounted->ids.empty()) {
        return {};
    }
    const ModuleDef* module = content.FindModule(mounted->ids.back());
    return module != nullptr ? module->displayName : mounted->ids.back().str();
}

bool CanMountHere(const entt::registry& registry, const core::ContentLibrary& content,
                  entt::entity targetRig, entt::entity mount, const ModuleId& module) {
    if (!registry.valid(mount)) {
        return false;
    }
    const ParentRig* parent = registry.try_get<ParentRig>(mount);
    if (parent == nullptr || parent->root != targetRig) {
        return false;
    }
    if (registry.all_of<Destroyed>(mount) || HardpointHoldsModules(registry, mount)) {
        return false;
    }
    const ShellRole* shellRole = registry.try_get<ShellRole>(mount);
    if (shellRole == nullptr) {
        return false;
    }
    const ModuleDef* moduleDef = content.FindModule(module);
    return moduleDef != nullptr && shellRole->Accepts(moduleDef->kind);
}

}  // namespace sr::space::ui::engineering_screen
