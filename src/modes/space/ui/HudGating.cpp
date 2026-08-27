#include "modes/space/ui/HudGating.h"

#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"

namespace sr::space::ui::hud_gating {
namespace {

// SensorRange::units is already zeroed by shared/rig/ModuleAttachment.cpp's RecomputeRigTotals
// the moment the last living Sensor-kind hardpoint dies (its Max-aggregation pass simply has
// nothing left to max over), so this needs no Destroyed walk of its own.
bool SensorOnline(const entt::registry& registry, entt::entity rigRoot) {
    const SensorRange* sensor = registry.try_get<SensorRange>(rigRoot);
    return sensor != nullptr && sensor->units > 0.0f;
}

// Mirrors shared/rig/ModuleAttachment.cpp's RecomputeRigTotals `crewed` walk: a living,
// non-turret hardpoint carrying CrewRating with a nonzero command roll. A turret's own crew
// (ShellRole::kind == Weapon) is scoped to that hardpoint alone and never counts toward the rig's
// command surface, the same exclusion RecomputeRigTotals applies.
bool CommandOnline(const entt::registry& registry, entt::entity rigRoot) {
    const Rig* rig = registry.try_get<Rig>(rigRoot);
    if (rig == nullptr) {
        return false;
    }
    for (const entt::entity hardpoint : rig->children) {
        if (registry.all_of<Destroyed>(hardpoint)) {
            continue;
        }
        const CrewRating* crew = registry.try_get<CrewRating>(hardpoint);
        if (crew == nullptr || crew->command <= 0.0f) {
            continue;
        }
        const ShellRole* role = registry.try_get<ShellRole>(hardpoint);
        if (role == nullptr || role->kind != ShellKind::Weapon) {
            return true;
        }
    }
    return false;
}

}  // namespace

std::string_view Label(HudSurface surface) {
    switch (surface) {
        case HudSurface::Sensor: return "SENSORS";
        case HudSurface::Comms: return "COMMS";
        case HudSurface::Command: return "COMMAND";
        case HudSurface::Construction: return "CONSTRUCTION";
        case HudSurface::Hyperdrive: return "HYPERDRIVE";
    }
    return "";
}

bool IsOnline(const entt::registry& registry, entt::entity rigRoot, HudSurface surface) {
    if (rigRoot == entt::null) {
        return false;
    }
    switch (surface) {
        case HudSurface::Sensor: return SensorOnline(registry, rigRoot);
        case HudSurface::Command: return CommandOnline(registry, rigRoot);
        // Comms, Construction, and Hyperdrive have no live component to read yet -- see this
        // file's header comment.
        case HudSurface::Comms:
        case HudSurface::Construction:
        case HudSurface::Hyperdrive: return false;
    }
    return false;
}

std::array<SlotStatus, kSurfaceOrder.size()> BuildSlots(const entt::registry& registry,
                                                        entt::entity rigRoot) {
    std::array<SlotStatus, kSurfaceOrder.size()> slots{};
    for (std::size_t i = 0; i < kSurfaceOrder.size(); ++i) {
        slots[i] = SlotStatus{kSurfaceOrder[i], IsOnline(registry, rigRoot, kSurfaceOrder[i])};
    }
    return slots;
}

}  // namespace sr::space::ui::hud_gating
