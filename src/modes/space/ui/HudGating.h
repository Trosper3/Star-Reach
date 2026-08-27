#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <entt/entity/registry.hpp>
#include <string_view>

// modes/space/ui/ -- HudGating (issue #234, architecture.md 13.5 group 2e, features.md 3.10):
// "a HUD surface exists exactly when a living module provides it... fixed slots that disable
// rather than disappear" -- BridgeView::AvailableTabs's living-hardpoint pattern, one layer out
// from Bridge tabs to the flight HUD's own module-gated button bar.
//
// Sensor, Comms, and Command are real, dynamic gates today. Construction and Hyperdrive are
// always offline, not a bug: `FacilityKind::Construction` does not exist in the enum yet
// (architecture.md 12.26), and `ModuleKind::Hyperdrive` is an enumerator with no live
// per-hardpoint component of its own (architecture.md: "WarpSystem's fuel/jump gate lands in
// P9-06; this just makes the kind exist"). There is no rolled stat either could read yet -- the
// slot stays fixed and simply never lights up until its own issue lands a live component, rather
// than this file inventing one ahead of that work.
//
// Comms reads shared/components/Comms.h's CommsRange the same way Sensor reads Targeting.h's
// SensorRange (architecture.md 13.3 finding S, 12.27; issue #235). `ModuleKind::Comms` itself has
// no authored content yet (features.md 3.10's Comms roster), so this slot is wired but still dark
// today -- the identical state Sensor shipped in before any Sensor module existed.
namespace sr::space::ui::hud_gating {

enum class HudSurface : std::uint8_t {
    Sensor,
    Comms,
    Command,
    Construction,
    Hyperdrive,
};

// Declaration order is draw order, fixed regardless of loadout -- features.md 3.10: "the bar
// never resizes as hardpoints die."
inline constexpr std::array<HudSurface, 5> kSurfaceOrder = {
    HudSurface::Sensor,       HudSurface::Comms,      HudSurface::Command,
    HudSurface::Construction, HudSurface::Hyperdrive,
};

// "SENSORS", "COMMS", "COMMAND", "CONSTRUCTION", "HYPERDRIVE" -- always shown, online or not, so
// a disabled slot states why rather than reading as empty (features.md 8.3's "absence must never
// look like emptiness").
std::string_view Label(HudSurface surface);

// True exactly when `rigRoot` has a living module backing `surface` right now. False for
// entt::null or a rig with no Rig component. Pure -- no raylib -- so unit-testable.
bool IsOnline(const entt::registry& registry, entt::entity rigRoot, HudSurface surface);

struct SlotStatus {
    HudSurface surface = HudSurface::Sensor;
    bool online = false;
};

// One SlotStatus per kSurfaceOrder entry, in that fixed order. The button bar this drives never
// reindexes or resizes -- only SlotStatus::online changes as modules live and die (issue #234's
// own test: "no HUD element changes position when a module dies").
std::array<SlotStatus, kSurfaceOrder.size()> BuildSlots(const entt::registry& registry,
                                                        entt::entity rigRoot);

}  // namespace sr::space::ui::hud_gating
