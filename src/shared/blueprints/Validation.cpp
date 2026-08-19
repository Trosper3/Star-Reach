#include "shared/blueprints/Validation.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sr {
namespace {

void Add(ValidationResult& result, ValidationRule rule, std::string message, MountId mount = {}) {
    result.errors.push_back({rule, std::move(message), std::move(mount)});
}

// Rule 9. Uniqueness across the content set is enforced where it can actually be seen -- the
// registry rejects a duplicate key at load. Here we can only check the blueprint in hand.
void CheckIdentity(const ShipBlueprint& bp, ValidationResult& result) {
    if (bp.id.empty()) {
        Add(result, ValidationRule::Identity, "Blueprint has no id.");
    }
    if (bp.schemaVersion <= 0 || bp.schemaVersion > kBlueprintSchemaVersion) {
        Add(result, ValidationRule::Identity,
            "Blueprint '" + bp.id.str() + "' declares schemaVersion " +
                std::to_string(bp.schemaVersion) + "; supported range is 1.." +
                std::to_string(kBlueprintSchemaVersion) + ".");
    }
    if (bp.rig.mounts.empty()) {
        Add(result, ValidationRule::StructuralShell,
            "Blueprint '" + bp.id.str() + "' declares no mounts.");
    }
}

// Rule 8. Runs first so every later check can assume a resolved def or skip the mount, which is
// what keeps the error list readable instead of cascading one typo into six failures.
void CheckIds(const ShipBlueprint& bp, const DefLibrary& library, ValidationResult& result) {
    std::unordered_set<std::string> seenMounts;
    for (const auto& mount : bp.rig.mounts) {
        if (mount.id.empty()) {
            Add(result, ValidationRule::Identity, "A mount has no id.");
        } else if (!seenMounts.insert(mount.id.str()).second) {
            Add(result, ValidationRule::Identity, "Duplicate mount id '" + mount.id.str() + "'.",
                mount.id);
        }
        if (library.FindShell(mount.shell) == nullptr) {
            Add(result, ValidationRule::UnresolvedId,
                "Mount '" + mount.id.str() + "' references unknown shell '" + mount.shell.str() +
                    "'.",
                mount.id);
        }
        for (const auto& moduleId : mount.modules) {
            if (library.FindModule(moduleId) == nullptr) {
                Add(result, ValidationRule::UnresolvedId,
                    "Mount '" + mount.id.str() + "' references unknown module '" + moduleId.str() +
                        "'.",
                    mount.id);
            }
        }
    }
}

// Rule 6. Exactly one root (empty attachedTo), and every other parent reference resolves.
void CheckMountParents(const ShipBlueprint& bp, ValidationResult& result) {
    std::unordered_set<std::string> ids;
    for (const auto& mount : bp.rig.mounts) {
        ids.insert(mount.id.str());
    }

    int roots = 0;
    for (const auto& mount : bp.rig.mounts) {
        if (mount.attachedTo.empty()) {
            ++roots;
            continue;
        }
        if (mount.attachedTo == mount.id) {
            Add(result, ValidationRule::MountParent,
                "Mount '" + mount.id.str() + "' is attached to itself.", mount.id);
        } else if (ids.find(mount.attachedTo.str()) == ids.end()) {
            Add(result, ValidationRule::MountParent,
                "Mount '" + mount.id.str() + "' is attached to '" + mount.attachedTo.str() +
                    "', which is not a mount in this rig.",
                mount.id);
        }
    }

    if (!bp.rig.mounts.empty() && roots != 1) {
        Add(result, ValidationRule::MountParent,
            "A rig needs exactly one root mount with no attachedTo; found " +
                std::to_string(roots) + ".");
    }
}

// Rule 7. Walks outward from the root and reports whatever the walk never reaches. Cycles
// surface here too: a disconnected ring is simply unreachable.
void CheckAdjacency(const ShipBlueprint& bp, ValidationResult& result) {
    if (bp.rig.mounts.empty()) {
        return;
    }

    std::unordered_map<std::string, std::vector<const MountBlueprint*>> childrenOf;
    const MountBlueprint* root = nullptr;
    for (const auto& mount : bp.rig.mounts) {
        if (mount.attachedTo.empty()) {
            if (root == nullptr) {
                root = &mount;
            }
        } else {
            childrenOf[mount.attachedTo.str()].push_back(&mount);
        }
    }
    if (root == nullptr) {
        return;  // Already reported by CheckMountParents.
    }

    std::unordered_set<std::string> reached;
    std::vector<const MountBlueprint*> stack{root};
    while (!stack.empty()) {
        const MountBlueprint* current = stack.back();
        stack.pop_back();
        if (!reached.insert(current->id.str()).second) {
            continue;
        }
        auto it = childrenOf.find(current->id.str());
        if (it != childrenOf.end()) {
            stack.insert(stack.end(), it->second.begin(), it->second.end());
        }
    }

    for (const auto& mount : bp.rig.mounts) {
        if (reached.find(mount.id.str()) == reached.end()) {
            Add(result, ValidationRule::Adjacency,
                "Mount '" + mount.id.str() +
                    "' is not connected to the rig root; floating islands are not buildable.",
                mount.id);
        }
    }
}

// Not enumerated in features.md section 2.3, but implied by ShellDef::moduleSlots and
// ShellDef::acceptsKinds. Both are mechanical consequences of the content model rather than
// design choices, and catching them here is what keeps RigFactory free of defensive branches.
void CheckMounting(const ShipBlueprint& bp, const DefLibrary& library, ValidationResult& result) {
    for (const auto& mount : bp.rig.mounts) {
        const ShellDef* shell = library.FindShell(mount.shell);
        if (shell == nullptr) {
            continue;  // Already reported by CheckIds.
        }

        if (static_cast<int>(mount.modules.size()) > shell->moduleSlots) {
            Add(result, ValidationRule::MountCapacity,
                "Mount '" + mount.id.str() + "' holds " + std::to_string(mount.modules.size()) +
                    " modules but shell '" + shell->id.str() + "' has " +
                    std::to_string(shell->moduleSlots) + " slot(s).",
                mount.id);
        }

        for (const auto& moduleId : mount.modules) {
            const ModuleDef* module = library.FindModule(moduleId);
            if (module != nullptr && !shell->Accepts(module->kind)) {
                Add(result, ValidationRule::ModuleCompatibility,
                    "Module '" + moduleId.str() + "' (" + std::string(ToString(module->kind)) +
                        ") cannot mount in a " + std::string(ToString(shell->kind)) +
                        " shell at '" + mount.id.str() + "'.",
                    mount.id);
            }
        }
    }
}

// True if any mount uses a shell of `kind` and carries at least one resolvable module. The
// "carrying a module" half matters: an empty engine housing is a hull decoration, not thrust.
bool HasEquippedShell(const ShipBlueprint& bp, const DefLibrary& library, ShellKind kind) {
    for (const auto& mount : bp.rig.mounts) {
        const ShellDef* shell = library.FindShell(mount.shell);
        if (shell == nullptr || shell->kind != kind) {
            continue;
        }
        for (const auto& moduleId : mount.modules) {
            if (library.FindModule(moduleId) != nullptr) {
                return true;
            }
        }
    }
    return false;
}

// Rules 1, 2 and 5.
void CheckRequiredShells(const ShipBlueprint& bp, const DefLibrary& library,
                         ValidationResult& result) {
    if (!HasEquippedShell(bp, library, ShellKind::Chassis) &&
        !HasEquippedShell(bp, library, ShellKind::Armor)) {
        Add(result, ValidationRule::StructuralShell,
            "A rig needs at least one chassis or armor shell with a module attached.");
    }
    if (!HasEquippedShell(bp, library, ShellKind::PowerCell)) {
        Add(result, ValidationRule::PowerSource,
            "A rig needs at least one power cell shell with a module attached.");
    }
    // Stations are exempt -- features.md validation rule 5.
    if (bp.mobile && !HasEquippedShell(bp, library, ShellKind::Engine)) {
        Add(result, ValidationRule::Propulsion,
            "A mobile vessel needs at least one engine shell with a module attached.");
    }
}

// A weapon-shell mount carrying a resolvable module must author a positive traverseRadians
// (architecture.md 13.3 finding W, 13.4 decision 5). Before this, an omitted field defaulted to
// the same 0.0f RigBlueprint.h documents as "fixed forward with no traverse" -- indistinguishable
// from a gun nobody remembered to aim, and the omission was silent.
void CheckWeaponTraverse(const ShipBlueprint& bp, const DefLibrary& library,
                         ValidationResult& result) {
    for (const auto& mount : bp.rig.mounts) {
        const ShellDef* shell = library.FindShell(mount.shell);
        if (shell == nullptr || shell->kind != ShellKind::Weapon) {
            continue;
        }
        bool hasWeaponModule = false;
        for (const auto& moduleId : mount.modules) {
            if (library.FindModule(moduleId) != nullptr) {
                hasWeaponModule = true;
                break;
            }
        }
        if (!hasWeaponModule) {
            continue;
        }
        if (mount.traverseRadians <= 0.0f) {
            Add(result, ValidationRule::WeaponTraverse,
                "Mount '" + mount.id.str() +
                    "' carries a weapon module but authors no traverseRadians.",
                mount.id);
        }
    }
}

// Rule 12. The union of structural hardpoints -- chassis plus armour -- must cover the hull
// envelope: a non-structural (not Chassis, not Armor) mount must hang directly off a structural
// one, never off another non-structural mount with no armour or chassis between them
// (features.md 3.2's diagram: turret and engine attach to armour, not to each other). Skips a
// mount whose own shell, or whose parent, does not resolve -- CheckIds/CheckMountParents already
// report those.
void CheckStructuralCoverage(const ShipBlueprint& bp, const DefLibrary& library,
                             ValidationResult& result) {
    std::unordered_map<std::string, const MountBlueprint*> byId;
    for (const auto& mount : bp.rig.mounts) {
        byId[mount.id.str()] = &mount;
    }

    for (const auto& mount : bp.rig.mounts) {
        if (mount.attachedTo.empty()) {
            continue;  // The root: nothing structural to check it against.
        }
        const ShellDef* shell = library.FindShell(mount.shell);
        if (shell == nullptr || shell->kind == ShellKind::Chassis ||
            shell->kind == ShellKind::Armor) {
            continue;
        }
        const auto it = byId.find(mount.attachedTo.str());
        if (it == byId.end()) {
            continue;  // Dangling parent -- already reported by CheckMountParents.
        }
        const ShellDef* parentShell = library.FindShell(it->second->shell);
        if (parentShell != nullptr && parentShell->kind != ShellKind::Chassis &&
            parentShell->kind != ShellKind::Armor) {
            Add(result, ValidationRule::StructuralCoverage,
                "Mount '" + mount.id.str() + "' hangs off '" + mount.attachedTo.str() +
                    "', a non-structural shell -- chassis and armour must cover the hull "
                    "envelope with no bare sections.",
                mount.id);
        }
    }
}

// Rules 3 and 4 -- the two design-facing ones. This is the constraints puzzle.
void CheckBudgets(const ShipBlueprint& bp, const DefLibrary& library, ValidationResult& result) {
    const RigTotals totals = ComputeTotals(bp, library);

    if (totals.PowerBalance() < 0.0f) {
        Add(result, ValidationRule::PowerBalance,
            "Power draw " + std::to_string(totals.powerDraw) + " exceeds generation " +
                std::to_string(totals.powerGeneration) + ".");
    }
    if (bp.structuralMassLimit > 0.0f && totals.mass > bp.structuralMassLimit) {
        Add(result, ValidationRule::MassLimit,
            "Rig mass " + std::to_string(totals.mass) + " exceeds the structural limit " +
                std::to_string(bp.structuralMassLimit) + ".");
    }
}

}  // namespace

std::string_view ToString(ValidationRule rule) {
    switch (rule) {
        case ValidationRule::StructuralShell: return "StructuralShell";
        case ValidationRule::PowerSource: return "PowerSource";
        case ValidationRule::PowerBalance: return "PowerBalance";
        case ValidationRule::MassLimit: return "MassLimit";
        case ValidationRule::Propulsion: return "Propulsion";
        case ValidationRule::MountParent: return "MountParent";
        case ValidationRule::Adjacency: return "Adjacency";
        case ValidationRule::UnresolvedId: return "UnresolvedId";
        case ValidationRule::Identity: return "Identity";
        case ValidationRule::MountCapacity: return "MountCapacity";
        case ValidationRule::ModuleCompatibility: return "ModuleCompatibility";
        case ValidationRule::WeaponTraverse: return "WeaponTraverse";
        case ValidationRule::StructuralCoverage: return "StructuralCoverage";
    }
    return "Unknown";
}

bool ValidationResult::HasRule(ValidationRule rule) const {
    for (const auto& error : errors) {
        if (error.rule == rule) {
            return true;
        }
    }
    return false;
}

RigTotals ComputeTotals(const ShipBlueprint& bp, const DefLibrary& library) {
    RigTotals totals;
    for (const auto& mount : bp.rig.mounts) {
        if (const ShellDef* shell = library.FindShell(mount.shell)) {
            totals.mass += shell->mass;
            totals.hull += shell->hull;
        }
        for (const auto& moduleId : mount.modules) {
            if (const ModuleDef* module = library.FindModule(moduleId)) {
                totals.mass += module->mass;
                totals.hull += module->hullBonus;
                totals.powerDraw += module->powerDraw;
                totals.powerGeneration += module->powerGeneration;
            }
        }
    }
    return totals;
}

ValidationResult Validate(const ShipBlueprint& bp, const DefLibrary& library) {
    ValidationResult result;
    CheckIdentity(bp, result);
    CheckIds(bp, library, result);
    CheckMountParents(bp, result);
    CheckAdjacency(bp, result);
    CheckMounting(bp, library, result);
    CheckWeaponTraverse(bp, library, result);
    CheckStructuralCoverage(bp, library, result);
    CheckRequiredShells(bp, library, result);
    CheckBudgets(bp, library, result);
    return result;
}

}  // namespace sr
