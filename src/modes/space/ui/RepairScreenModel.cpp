// RepairScreen.cpp's own pure data half, split out to satisfy architecture.md 2.2's 600-line file
// cap (vertical-scroll support pushed the drawing half well past it on its own) -- the same split
// StorageScreen.cpp/StorageScreenModel.cpp and ModulesMenu.cpp/ModulesMenuModel.cpp already
// establish. Everything here carries no raylib and no layout, so it costs nothing to keep in its
// own translation unit, unlike Update()/Draw()'s input polling and layout math, which stay with
// the widgets they position.
#include "modes/space/ui/RepairScreen.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"

namespace sr::space::ui::repair_screen {
namespace {

// One-letter monogram per ShellKind (features.md 3.9's glyph-carries-identity rule) -- the same
// placeholder RefactorMenu.cpp's own ShellGlyph uses; duplicated locally rather than shared,
// since neither file may depend on the other and this is the whole of it.
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

}  // namespace

std::vector<RepairRow> Rows(const entt::registry& registry, entt::entity subject, bool hasOrder,
                            entt::entity orderedHardpoint, int costPerHp) {
    std::vector<RepairRow> rows;
    const Rig* rig = registry.try_get<Rig>(subject);
    if (rig == nullptr) {
        return rows;
    }
    for (const entt::entity hardpoint : rig->children) {
        const Health* health = registry.try_get<Health>(hardpoint);
        if (health == nullptr) {
            continue;
        }
        RepairRow entry;
        entry.hardpoint = hardpoint;
        entry.destroyed = registry.all_of<Destroyed>(hardpoint);
        entry.ordered = !entry.destroyed && hasOrder &&
                        (orderedHardpoint == entt::null || orderedHardpoint == hardpoint);

        if (const ShellRole* role = registry.try_get<ShellRole>(hardpoint)) {
            ShellGlyph(role->kind, entry.row.glyph);
        }
        if (const MountRef* mount = registry.try_get<MountRef>(hardpoint)) {
            entry.row.label = mount->id.str();
        }
        entry.row.style.integrity = health->max > 0.0f ? health->current / health->max : 0.0f;
        entry.row.style.disabled = entry.destroyed;

        const std::string hull = std::to_string(static_cast<int>(health->current)) + "/" +
                                 std::to_string(static_cast<int>(health->max));
        if (entry.destroyed) {
            entry.row.value = hull + "  REBUILD (P4-05)";
        } else {
            const float missing = std::max(0.0f, health->max - health->current);
            entry.costToFull = static_cast<int>(std::ceil(missing * static_cast<float>(costPerHp)));
            entry.row.value = hull + "  " + std::to_string(entry.costToFull) + "cr";
            if (entry.ordered) {
                entry.row.value += "  [STOP]";
            }
        }
        rows.push_back(std::move(entry));
    }
    // architecture.md 12.30.4: "the thing most likely to kill you is the first row."
    std::sort(rows.begin(), rows.end(), [](const RepairRow& a, const RepairRow& b) {
        return a.row.style.integrity < b.row.style.integrity;
    });
    return rows;
}

bool RepairAllActive(bool hasOrder, entt::entity orderedHardpoint) {
    return hasOrder && orderedHardpoint == entt::null;
}

int FacilityGrade(const entt::registry& registry, entt::entity facilityHardpoint) {
    const FacilityRef* facility = registry.try_get<FacilityRef>(facilityHardpoint);
    return facility != nullptr ? facility->grade : 1;
}

float FacilityRate(const entt::registry& registry, const core::ContentLibrary& content,
                   entt::entity facilityHardpoint) {
    const MountedModules* mounted = registry.try_get<MountedModules>(facilityHardpoint);
    if (mounted == nullptr) {
        return 0.0f;
    }
    float rate = 0.0f;
    for (const ModuleId& id : mounted->ids) {
        const ModuleDef* module = content.FindModule(id);
        if (module != nullptr && module->facility.kind == FacilityKind::Repair) {
            rate = module->facility.ratePerSecond;
            break;
        }
    }
    if (rate <= 0.0f) {
        return 0.0f;
    }
    if (const ParentRig* parent = registry.try_get<ParentRig>(facilityHardpoint)) {
        if (const CrewRepairBonus* bonus = registry.try_get<CrewRepairBonus>(parent->root)) {
            rate *= (1.0f + bonus->value);
        }
    }
    return rate;
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

}  // namespace sr::space::ui::repair_screen
