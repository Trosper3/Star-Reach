#include "modes/space/ui/FlightControls.h"

#include <raylib.h>

#include <cstdint>

#include "core/events/Intent.h"
#include "modes/space/render/IconRenderer.h"

namespace sr::space::ui::flight_controls {
namespace {

// Index order matches the keyboard row, not numeric order -- 1-9 then 0 -- so group 10 lands on
// the key to the right of group 9 (features.md 3.6).
constexpr int kWeaponGroupKeys[10] = {KEY_ONE, KEY_TWO,   KEY_THREE, KEY_FOUR, KEY_FIVE,
                                      KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE, KEY_ZERO};

// Settled weapon-group enable state: flipped on each key's up-edge (IsKeyPressed, so holding a
// key toggles once rather than chattering), then pushed as one absolute value every frame --
// SetWeaponGroupsIntent's own comment (core/events/Intent.h) explains why an edge-triggered
// "flip bit i" push is not safe under this queue's real lifecycle. Static, matching
// AvionicsMenu.cpp's gHeldSeconds precedent: this is input-layer bookkeeping for the single local
// player Poll() is ever called for, not game state -- EnabledWeaponGroups on the rig itself
// remains the actual source of truth PlayerInputSystem/WeaponSystem read.
std::uint16_t gWeaponGroupMask = 0x03FFu;

}  // namespace

void Poll(core::IntentQueue& out, ActorId self, const render::CameraView& camera) {
    core::SetThrottleIntent throttle;
    throttle.actor = self;
    throttle.forward = static_cast<float>(IsKeyDown(KEY_W)) - static_cast<float>(IsKeyDown(KEY_S));
    throttle.strafe = static_cast<float>(IsKeyDown(KEY_E)) - static_cast<float>(IsKeyDown(KEY_Q));
    throttle.turn = static_cast<float>(IsKeyDown(KEY_D)) - static_cast<float>(IsKeyDown(KEY_A));
    out.Push(throttle);

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        out.Push(core::FireWeaponsIntent{self});
    }

    const Vector2 mouse = GetMousePosition();
    out.Push(core::AimIntent{self, render::ScreenToWorld(Vec2{mouse.x, mouse.y}, camera)});

    for (int i = 0; i < 10; ++i) {
        if (IsKeyPressed(kWeaponGroupKeys[i])) {
            gWeaponGroupMask ^= static_cast<std::uint16_t>(1u << i);
        }
    }
    out.Push(core::SetWeaponGroupsIntent{self, gWeaponGroupMask});
}

}  // namespace sr::space::ui::flight_controls
