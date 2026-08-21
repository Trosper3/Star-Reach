#pragma once

#include "shared/blueprints/Taxonomy.h"

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system (or, here, modes/space/ui/) under modes/space/.
namespace sr {

// Which FacilityKind this hardpoint provides once powered, mirroring ModuleDef::facility.kind
// (RigFactory.cpp's AttachModule) -- the live-side half of features.md section 4's "component-
// driven menus": destroying this hardpoint (or its module never getting attached) is what
// removes a Bridge tab mid-session (modes/space/ui/BridgeView.h). DockingBay (Docking.h) already
// carries FacilityKind::Docking as its own tag for DockingSystem's direct use; this is the
// generic version every FacilityKind gets, Docking included.
struct FacilityRef {
    FacilityKind kind = FacilityKind::Repair;
    // Copied from ModuleDef::facility.grade at attach time. Meaningful only for
    // FacilityKind::Engineering -- see ModuleDef.h's FacilityStats::grade comment.
    int grade = 1;
};

}  // namespace sr
