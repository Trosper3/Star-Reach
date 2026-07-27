#pragma once

#include <entt/entity/entity.hpp>

// Components are plain-old-data (Law 1): no virtual methods, no inheritance, no owning pointers,
// and no std::vector where a child entity would do (Law 4). Anything that looks like behavior
// belongs in a system under modes/space/systems/.
namespace sr {

// Tag: this hardpoint is a docking bay. Attached by RigFactory when a mounted module's
// FacilityStats.kind is FacilityKind::Docking -- the first live consumer of ModuleKind::Facility,
// which RigFactory previously attached no components for at all.
struct DockingBay {};

// Set by input or AI; consumed and cleared by DockingSystem the same tick, the same idiom as
// Combat.h's FireIntent. `bay` must be the DockingBay hardpoint DockPrompt currently names, or
// the request is silently dropped -- it is not re-validated against a wider search.
struct DockRequest {
    entt::entity bay = entt::null;
};

// Set by input; consumed and cleared by DockingSystem the same tick.
struct UndockRequest {};

// Tag + state: this rig root is docked. Untargetable and immobile while present -- Targeting.h
// already documents "a docked player is untargetable" as the rule DockingSystem enforces.
struct Docked {
    entt::entity station = entt::null;
    entt::entity bay = entt::null;
};

// Recomputed every tick: the nearest in-range, same-faction DockingBay this rig could dock at
// right now, for a future UI to read and show a "Press [DOCK]" prompt. Absent when nothing is in
// range -- a rig that drifts away must stop prompting the same tick it leaves, not linger.
struct DockPrompt {
    entt::entity bay = entt::null;
};

}  // namespace sr
