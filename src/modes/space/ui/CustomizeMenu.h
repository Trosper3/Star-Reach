#pragma once

#include "core/events/Intent.h"
#include "core/events/IntentQueue.h"
#include "core/knowledge/KnowledgeNetwork.h"
#include "shared/blueprints/ShipBlueprint.h"
#include "shared/blueprints/Validation.h"

// modes/space/ui/CustomizeMenu -- architecture.md 12.9 (features.md 2.2-2.3), Template creation.
//
// modes/*/ui/ must not include systems/ (section 2.3) -- this menu never touches one. The wizard
// assembles a draft ShipBlueprint in local UI state (not a component, not persisted) and, on
// Save, emits a SaveTemplateIntent (Law 9) rather than calling KnowledgeStore::Grant directly.
//
// Every draft-mutating function here builds ShipBlueprint/RigBlueprint/MountBlueprint by plain
// declaration and field assignment, never an aggregate initializer -- tools/ci/
// check_content_pipeline.py (Law 10) forbids constructing those types with an initializer
// outside core/registries/, tests/, and tools/. A player's Template draft is real authored
// content, so it goes through the same "declare, then assign" shape those files already use,
// not a `ShipBlueprint{...}` literal.
namespace sr::space::ui::customize_menu {

// A fresh, empty draft: no id, no mounts, structuralMassLimit 0.
ShipBlueprint NewDraft();

// Appends one mount. `attachedTo` must be empty for the rig's root mount (RigBlueprint.h's
// "empty on exactly one mount" rule) and a prior mount's id otherwise. Does not validate
// structural adjacency itself -- that is Validation.h's job, run by CanSave below, so a
// mid-assembly draft with a dangling `attachedTo` is allowed to exist while the wizard is
// still being built.
void AddMount(ShipBlueprint& draft, const MountId& id, const ShellId& shell,
              const MountId& attachedTo, Vec2 localOffset);

// Attaches `module` to the mount named `mountId`. No-op if the mount does not exist in `draft`.
void EquipModule(ShipBlueprint& draft, const MountId& mountId, const ModuleId& module);

// Runs Validation.h against `draft`. True only when every rule in Validate()'s result passes --
// "an invalid draft cannot be saved" (architecture.md 12.9's test bullet).
bool CanSave(const ShipBlueprint& draft, const DefLibrary& library);

// Builds the Intent CustomizeMenu's Save action pushes onto the IntentQueue. Does not itself
// check CanSave -- callers should, so the UI can report which rule failed before ever building
// this; ConsumeSaveTemplateRequests below re-checks regardless (defense in depth against a stale
// or modified client).
core::SaveTemplateIntent BuildSaveRequest(core::ActorId actor,
                                          const KnowledgeNetworkId& targetNetwork,
                                          const ShipBlueprint& draft);

// The consumer architecture.md 12.9 names: "a store call, not a tick, so no new system owns it."
// Grants every pending SaveTemplateIntent whose blueprint still validates against `library` into
// its targetNetwork; silently drops one that does not (the draft simply never round-trips into
// the network -- there is no UI channel here to report back to, since this runs off the shared
// IntentQueue rather than a per-menu callback). Called once per frame alongside this menu's
// Draw/Update, the same way BridgeView's Draw is -- not from SystemSchedule.cpp, because Law 8
// store access is not a tick (architecture.md section 4).
void ConsumeSaveTemplateRequests(const core::IntentQueue& intents,
                                 core::knowledge::KnowledgeStore& knowledge,
                                 const DefLibrary& library);

// Draws the wizard: current mounts, validation errors, and a Save button gated on CanSave.
void Draw(const ShipBlueprint& draft, const DefLibrary& library);

}  // namespace sr::space::ui::customize_menu
