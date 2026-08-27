#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::tutorial_system {

// Act I's four lessons, the prologue's commander offer, and the anomaly's cataclysm
// (features.md 1.2, architecture.md section 4 / 13.1's TutorialSystem row).
//
//   - Offer: the fixed prologue system (modes/space/factories/WorldGen.cpp's
//     PopulatePrologueSystem) is the only registry that ever carries an AnomalyField
//     (shared/components/Physics.h). The first tick that sees one and a PlayerControlled rig
//     with neither Tutorial, TutorialOffer, nor TutorialDeclined logs the commander's hail to
//     CommsLog and emplaces TutorialOffer on the player -- modes/space/ui/TutorialHud.h is the
//     only reader, and resolves it to Tutorial (Accept) or TutorialDeclined (Decline).
//   - Move: the rig has traveled kTutorialMoveDistance from its position when the tutorial
//     started (captured lazily on the first tick TutorialSystem sees it).
//   - Target: the rig has a live Target (Targeting.h).
//   - Fire: any weapon hardpoint on the rig has a non-zero Weapon::cooldown (Combat.h) --
//     WeaponSystem only sets this when a shot actually leaves the mount, and it persists for
//     fireIntervalSeconds afterward, long enough for this system (which runs after WeaponSystem)
//     to observe it reliably instead of racing a same-tick Projectile that ProjectileSystem might
//     already have consumed.
//   - Equip: the rig's live mounted-module count (summed across every hardpoint's
//     MountedModules, Rig.h) exceeds Tutorial::startEquippedModules -- captured lazily alongside
//     startPosition, since every blueprint already arrives with modules mounted and "any
//     MountedModules is non-empty" would complete this before the player did anything.
//   - Cataclysm: regardless of whether the offer was accepted or declined (features.md 1.2:
//     "declining changes nothing else"), the PlayerControlled rig crossing an AnomalyField's
//     triggerRadius emplaces a SystemWarpRequest (shared/components/Warp.h) bound for a
//     pseudo-randomly named frontier system -- SpaceFlight's existing SystemWarpRequest handoff
//     (issue #96) does the rest. Guarded on the player not already carrying one, both so a
//     repeated Tick before SpaceFlight processes it can't overwrite the target, and so the event
//     is structurally one-shot: the request tears down this whole registry, anomaly included.
//
// Act II's Gather/Dock-and-trade/Customize/Build/Warp lessons and its gathering tithe are NOT
// implemented here -- they land with P6-13, against Tutorial::act == Act::Two, which nothing
// produces yet.
void Tick(const SystemContext& ctx);

}  // namespace sr::space::tutorial_system
