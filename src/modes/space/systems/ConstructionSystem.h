#pragma once

#include "modes/space/systems/System.h"

namespace sr::space::construction_system {

// Player-initiated station/ship construction (architecture.md 12.12's BuildMenu), Tier 1.
//
// Consumes every BuildStationRequest/PlaceShipRequest (shared/components/Construction.h) the
// same tick they're set, the same idiom DockingSystem already uses for DockRequest. This is the
// one system in the 12.9-12.12 batch that legitimately calls a factory directly: construction
// IS assembly (Law 5), not a simulation step reaching past its layer -- tools/ci/
// check_layers.py carries a narrow, named exemption for this file's factories/ includes,
// documented there per section 11.7.
//
// Refused (request cleared, nothing else happens) when the requester's Wallet can't afford
// `cost`, the blueprint is a drafted Template the requester's own NetworkOwner does not hold
// (architecture.md 12.30.8's knowledge gate -- authored base-game content needs no membership),
// or the blueprint is unknown/invalid/wrong `mobile` flag for the request kind (the factory's own
// precondition -- StationFactory::Spawn rejects `mobile: true`, RigFactory::Spawn accepts
// either). Spend happens only after the factory reports success, so a refused build never costs
// anything -- no refund path needed.
//
// Also runs SaveTemplateIntent's consumer (architecture.md 12.9, 12.30.8) -- registers a valid
// draft's body into ContentLibrary's Template overlay and grants its id into the target network,
// closing the loop that makes a saved Template buildable at all. Moved here from
// modes/space/ui/CustomizeMenu.cpp: mutating ContentLibrary is core/ state a UI file may not
// touch (Law 9).
void Tick(const SystemContext& ctx);

}  // namespace sr::space::construction_system
