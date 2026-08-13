#include "modes/space/ui/FlightControls.h"

#include <raylib.h>

#include "core/events/Intent.h"

namespace sr::space::ui::flight_controls {

void Poll(core::IntentQueue& out, ActorId self) {
    core::SetThrottleIntent throttle;
    throttle.actor = self;
    throttle.forward = static_cast<float>(IsKeyDown(KEY_W)) - static_cast<float>(IsKeyDown(KEY_S));
    throttle.strafe = static_cast<float>(IsKeyDown(KEY_E)) - static_cast<float>(IsKeyDown(KEY_Q));
    throttle.turn = static_cast<float>(IsKeyDown(KEY_D)) - static_cast<float>(IsKeyDown(KEY_A));
    out.Push(throttle);

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        out.Push(core::FireWeaponsIntent{self});
    }
}

}  // namespace sr::space::ui::flight_controls
