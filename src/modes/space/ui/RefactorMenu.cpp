#include "modes/space/ui/RefactorMenu.h"

#include <algorithm>

#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/ui/Widgets.h"

namespace sr::space::ui::refactor_menu {
namespace {

// One-letter monogram per ShellKind (features.md 3.9's glyph-carries-identity rule), the same
// placeholder shape IconRenderer's category monograms use -- no per-shell art exists yet.
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

std::vector<entt::entity> DeletableHardpoints(const entt::registry& registry,
                                              entt::entity rigRoot) {
    std::vector<entt::entity> deletable;
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return deletable;
    }

    for (const entt::entity hardpoint : rig->children) {
        bool hasDependent = false;
        for (const entt::entity other : rig->children) {
            if (other == hardpoint) {
                continue;
            }
            const StructuralAttachment* attachment = registry.try_get<StructuralAttachment>(other);
            if (attachment != nullptr && attachment->attachedTo == hardpoint) {
                hasDependent = true;
                break;
            }
        }
        if (!hasDependent) {
            deletable.push_back(hardpoint);
        }
    }
    return deletable;
}

DeleteHardpointRequest BuildDeleteRequest(entt::entity hardpoint) {
    DeleteHardpointRequest request;
    request.hardpoint = hardpoint;
    return request;
}

void Draw(const Rectangle& bounds, const entt::registry& registry, entt::entity rigRoot) {
    const Rectangle content = sr::ui::DrawPanelFrame(bounds);

    const std::vector<entt::entity> deletable = DeletableHardpoints(registry, rigRoot);
    const Rig* rig = registry.try_get<Rig>(rigRoot);

    std::vector<sr::ui::Row> rows;
    if (rig != nullptr) {
        rows.reserve(rig->children.size());
        for (const entt::entity hardpoint : rig->children) {
            sr::ui::Row row;
            if (const MountRef* mount = registry.try_get<MountRef>(hardpoint)) {
                row.label = mount->id.str();
            }
            if (const ShellRole* role = registry.try_get<ShellRole>(hardpoint)) {
                ShellGlyph(role->kind, row.glyph);
            }
            row.style.disabled =
                std::find(deletable.begin(), deletable.end(), hardpoint) == deletable.end();
            rows.push_back(row);
        }
    }
    sr::ui::DrawListView(content, rows, 0.0f, "NO HARDPOINTS");
}

}  // namespace sr::space::ui::refactor_menu
