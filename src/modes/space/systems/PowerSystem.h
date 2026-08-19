#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::power_system {

// Resolves each rig root's PowerBudget from its living hardpoints' PowerSource/PowerLoad against
// the four player-commandable categories (features.md section 2.9, architecture.md 12.16 item
// 18): weapons, shields, engines, facilities, each at Offline/Reduced/Normal/Boosted per
// PowerAllocation (defaults to all-Normal if the rig carries none). Each hardpoint's own
// PowerLevels (cached from ModuleDef at attach time) scales its authored draw and its category's
// effect multiplier at that level -- Boosted can exceed 1.0, Offline is exactly 0.
//
// Under headroom, every commanded level is funded as requested. Over headroom, a commanded
// Boosted category is refused outright (clamped back to Normal for this tick) rather than
// shedding something else to fund it -- "boost simply does not engage without headroom." Only if
// that alone still does not fit (a genuine generation shortfall, e.g. a dead power cell -- kept
// as a separate path from the allocation-overcommit refusal above) does it fall back to shedding
// whole categories to Offline, in the order the rig's PowerPriorityList configures (defaulting to
// facilities/shields/weapons/engines, the same order PowerLoad::priority used to fix by ModuleKind
// before it became player-configurable), tagging every hardpoint in a shed category with
// PowerShed.
//
// Must run before WeaponSystem and PhysicsSystem, which scale their output by
// PowerBudget::weapons/engines respectively (architecture.md section 11.3 point 4).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::power_system
