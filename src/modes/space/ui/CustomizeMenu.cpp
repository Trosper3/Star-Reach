#include "modes/space/ui/CustomizeMenu.h"

#include <raylib.h>

#include "shared/ui/HudTheme.h"

namespace sr::space::ui::customize_menu {

ShipBlueprint NewDraft() {
    ShipBlueprint draft;
    return draft;
}

void AddMount(ShipBlueprint& draft, const MountId& id, const ShellId& shell,
              const MountId& attachedTo, Vec2 localOffset) {
    MountBlueprint mount;
    mount.id = id;
    mount.shell = shell;
    mount.attachedTo = attachedTo;
    mount.localOffset = localOffset;
    draft.rig.mounts.push_back(mount);
}

void EquipModule(ShipBlueprint& draft, const MountId& mountId, const ModuleId& module) {
    for (MountBlueprint& mount : draft.rig.mounts) {
        if (mount.id == mountId) {
            mount.modules.push_back(module);
            return;
        }
    }
}

bool CanSave(const ShipBlueprint& draft, const DefLibrary& library) {
    return Validate(draft, library).ok();
}

core::SaveTemplateIntent BuildSaveRequest(core::ActorId actor,
                                          const KnowledgeNetworkId& targetNetwork,
                                          const ShipBlueprint& draft) {
    core::SaveTemplateIntent intent;
    intent.actor = actor;
    intent.targetNetwork = targetNetwork;
    intent.blueprint = draft;
    return intent;
}

void ConsumeSaveTemplateRequests(const core::IntentQueue& intents,
                                 core::knowledge::KnowledgeStore& knowledge,
                                 const DefLibrary& library) {
    intents.ForEach<core::SaveTemplateIntent>([&](const core::SaveTemplateIntent& request) {
        if (!CanSave(request.blueprint, library)) {
            return;
        }
        knowledge.Grant(request.targetNetwork, core::knowledge::NetworkEntryKind::SavedTemplate,
                        request.blueprint.id.str());
    });
}

void Draw(const ShipBlueprint& draft, const DefLibrary& library) {
    const ValidationResult result = Validate(draft, library);

    const Rectangle bounds{40.0f, 40.0f, 360.0f,
                           40.0f + static_cast<float>(draft.rig.mounts.size()) * 20.0f +
                               static_cast<float>(result.errors.size()) * 16.0f};
    sr::ui::DrawBracketPanel(bounds, sr::ui::kPanelGlass, sr::ui::kPanelChrome, 10.0f, 2.0f);

    DrawText(draft.displayName.empty() ? "New Template" : draft.displayName.c_str(),
             static_cast<int>(bounds.x) + 12, static_cast<int>(bounds.y) + 8, 16,
             sr::ui::kValueBright);

    for (std::size_t i = 0; i < draft.rig.mounts.size(); ++i) {
        const MountBlueprint& mount = draft.rig.mounts[i];
        DrawText(mount.id.str().c_str(), static_cast<int>(bounds.x) + 12,
                 static_cast<int>(bounds.y) + 32 + static_cast<int>(i) * 20, 12, sr::ui::kLabelDim);
    }

    int errorY = static_cast<int>(bounds.y) + 32 + static_cast<int>(draft.rig.mounts.size()) * 20;
    for (const ValidationError& error : result.errors) {
        const std::string ruleName(ToString(error.rule));
        DrawText(ruleName.c_str(), static_cast<int>(bounds.x) + 12, errorY, 12,
                 sr::ui::kStatusCritical);
        errorY += 16;
    }

    if (result.ok()) {
        DrawText("SAVE", static_cast<int>(bounds.x) + 12,
                 static_cast<int>(bounds.y + bounds.height) + 8, 14, sr::ui::kStatusGood);
    }
}

}  // namespace sr::space::ui::customize_menu
