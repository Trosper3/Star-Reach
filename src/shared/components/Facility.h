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
    // Copied from ModuleDef::facility.capacity at attach time (architecture.md 12.30.2, .6).
    // First reader is docking-bay occupancy; ResearchSystem's concurrent-job cap is the second,
    // and the same meaning both times -- "how many units of work this facility holds at once." 0
    // means unlimited, matching CargoHold::capacity's existing convention so one number does not
    // mean two things in two components.
    int capacity = 0;
};

}  // namespace sr
