#pragma once

#include <raylib.h>

#include "shared/blueprints/Ids.h"
#include "shared/components/Construction.h"
#include "shared/components/Loot.h"
#include "shared/math/Vec2.h"

// modes/space/ui/BuildMenu -- architecture.md 12.12: player-initiated station/ship construction.
// Reads the requester's own CargoHold/Wallet for affordability, then builds a
// BuildStationRequest/PlaceShipRequest (shared/components/Construction.h) for the caller to
// place on the requester -- the same DockRequest idiom AvionicsMenu already uses. modes/*/ui/
// must not include systems/ (section 2.3); this never calls
// modes/space/systems/ConstructionSystem directly.
namespace sr::space::ui::build_menu {

// True when `wallet` can afford `cost` -- what Draw gates the Build button on before it ever
// builds a request. Pure -- no raylib -- so unit-testable.
bool CanAfford(const Wallet& wallet, int cost);

BuildStationRequest BuildStationBuildRequest(const BlueprintId& blueprint, Vec2 position,
                                             float rotation, int cost);
PlaceShipRequest BuildPlaceShipRequest(const BlueprintId& blueprint, Vec2 position, float rotation,
                                       int cost);

void Draw(const Rectangle& bounds, const Wallet& wallet, int cost);

}  // namespace sr::space::ui::build_menu
