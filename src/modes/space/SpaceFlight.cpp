#include "modes/space/SpaceFlight.h"

#include "modes/space/render/WorldRenderer.h"
#include "modes/space/systems/SystemSchedule.h"
#include "modes/space/ui/AvionicsMenu.h"
#include "modes/space/ui/BridgeView.h"
#include "modes/space/ui/CockpitHud.h"

namespace sr::space {

SpaceFlight::SpaceFlight(const core::ContentLibrary& content,
                         core::economy::FactionEconomy& economy)
    : world_("sol", "Sol"), content_(content), economy_(economy) {}

void SpaceFlight::OnEnter() {
    // Factories populate the world here once they land (first vertical slice, step 5-7):
    // WorldGen seeds the system, ShipFactory instantiates the player rig from its blueprint,
    // NpcFactory adds the opposition. Nothing is constructed inline in this file -- Law 5.
}

void SpaceFlight::Update(float realDeltaSeconds) {
    // Polled once per real frame, same as the window itself -- IsKeyPressed's "pressed this
    // frame" state does not survive being checked mid-tick, so this runs before the fixed-step
    // loop rather than inside it. The DockRequest/UndockRequest it may write is still visible to
    // every tick this frame runs (Law 9's established idiom; see AvionicsMenu.h).
    ui::avionics_menu::Update(world_.Registry());

    clock_.Advance(realDeltaSeconds);

    while (clock_.ConsumeStep()) {
        const SystemContext ctx{
            world_, intents_, content_, core::kFixedDeltaSeconds, clock_.ElapsedTicks(), &economy_};
        RunTick(ctx);
    }

    // Drained after the whole schedule, never mid-list: a system's view of this tick's input
    // must not depend on its position in the order.
    intents_.Clear();
}

void SpaceFlight::Draw() const {
    // Camera math belongs in this file (Law 7); the draw calls themselves belong in
    // modes/space/render/.
    render::DrawWorld(world_, render::CameraView{cameraTarget_, cameraZoom_}, InterpolationAlpha());

    // modes/space/ui/ -- screen-space, outside DrawWorld's BeginMode2D/EndMode2D.
    ui::cockpit_hud::Draw(world_.Registry());
    ui::avionics_menu::Draw(world_.Registry());
    ui::bridge_view::Draw(world_.Registry());
}

void SpaceFlight::OnExit() {}

}  // namespace sr::space
