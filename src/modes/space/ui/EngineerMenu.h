#pragma once

#include <raylib.h>

#include "shared/components/Engineer.h"

// modes/space/ui/EngineerMenu -- architecture.md 12.12: merge two owned modules of the same
// ModuleKind into one. modes/*/ui/ must not include systems/ (section 2.3); this builds
// MergeModulesRequest (shared/components/Engineer.h) for the caller to place on the docked
// requester, the same DockRequest idiom AvionicsMenu already uses, and never calls
// modes/space/systems/EngineerSystem directly.
namespace sr::space::ui::engineer_menu {

MergeModulesRequest BuildMergeRequest(const ModuleId& primary, const ModuleId& secondary);

void Draw(const Rectangle& bounds, const ModuleId& primary, const ModuleId& secondary);

}  // namespace sr::space::ui::engineer_menu
