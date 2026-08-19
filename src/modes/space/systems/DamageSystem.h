#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::damage_system {

// Regenerates shields, drains every hardpoint's PendingDamage against whichever shield actually
// covers it per its ShieldCoverage mode (Personal/Bubble/Conformal, architecture.md 12.22),
// through architecture.md 12.33's generic damage-type effect table (shield typing, bypass, and
// Ion's always-absorbed/no-hull/power-drain shape per features.md section 3.1), and destroys any
// hardpoint whose hull reaches zero. Destruction then cascades along StructuralAttachment:
// severing a structural parent takes every child hanging off it with it, with no special case for
// the chassis -- it dies like anything else, and its death is rig death only because everything
// ultimately attaches to it. Also detects rig death: a rig with no living hardpoint left loses
// Targetable and is marked Destroyed itself -- there is no protected core (Capital Ship Design).
// A host's death then cascades to every rig docked to it (Docked.station), tagging each Destroyed
// and untargetable the same tick -- features.md 3.4's "dies with its host" (architecture.md 12.34),
// reusing LootSystem's existing combat-kill DeathWreck path once it runs next rather than a second
// one.
//
// Also invalidates a rig's Propulsion once its last living Engine-kind hardpoint dies. This is
// an all-or-nothing zeroing, not a proportional recompute: doing that properly needs a
// per-hardpoint engine-contribution component that does not exist yet, so today a rig with two
// engines keeps full thrust until BOTH are gone rather than losing half after the first.
//
// Must run LAST among simulation systems (SystemSchedule.h) -- destruction is the tick's final
// word, so no system downstream spends work on a hardpoint that is about to stop existing.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::damage_system
