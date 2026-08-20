#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::capture_system {

// Ownership transfer of an uncrewed hull -- features.md 3.2's capture, settled 2026-08-09/11 as
// boarding-in-place. Docked capture was explicitly left out of DockingSystem (see its own header)
// pending the crew shell (P2-06) and a "disabled" concept; both now exist, so this is the file
// that fills the gap DockingSystem.h names.
//
//   - Eligibility: a target is boardable while it is Uncrewed (features.md 3.2's disabled hull --
//     no living crew to resist with), still Targetable (alive, undocked -- the same "physical,
//     collidable, targetable object" the spec keeps it as), and hostile to the boarder. A crewed
//     hull is never eligible, full stop -- Ion-suppressed-but-crewed capture (features.md 9's
//     stale wording) is not this system's rule; see the .cpp for why.
//   - Hold: any Targetable, crewed (not itself Uncrewed), undocked rig within boarding range of an
//     eligible target accrues BoardingHold.heldSeconds. Leaving range, the target losing
//     eligibility, or switching to a different target resets the hold to zero -- the same
//     exposure-for-duration trade every other stand-still mechanic in this design uses.
//   - Transfer: once the hold completes, the target's FactionRef flips to the boarder's. Nothing
//     else on the rig carries its own FactionRef (DockingSystem's same-faction bay check already
//     relies on that being root-only), so every surviving hardpoint -- crew included -- transfers
//     for free. A Commander aboard has its own `faction` field updated to match.
//   - Symmetric by construction (features.md 6.3): the loop has no PlayerControlled/PlayerLocation
//     branch, so an AI faction boards the player's uncrewed hull through the identical path.
//
// NOT built here: Troop Bay and Tractor Beam (features.md 3.2's capacity-scaled hold duration and
// non-destructive "cannot flee" route) -- neither module exists yet (zero occurrences in
// data/base_game/modules.json), so every boarder holds for the same fixed duration until a later
// issue adds them. No HUD progress bar either -- features.md 3.10's "a HUD surface exists exactly
// when a living module provides it" names Troop Bay as that surface's provider, so the HUD element
// waits on the same module.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::capture_system
