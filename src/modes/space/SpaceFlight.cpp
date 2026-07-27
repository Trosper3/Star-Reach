#include "modes/space/SpaceFlight.h"

#include "modes/space/render/IconRenderer.h"
#include "modes/space/render/WorldRenderer.h"
#include "modes/space/systems/SystemSchedule.h"

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

void SpaceFlight::Draw(float alpha) const {
    // Camera math belongs in this file (Law 7); the draw calls themselves belong in
    // modes/space/render/.
    const render::CameraView camera{cameraTarget_, cameraZoom_};
    render::DrawWorld(world_, camera, alpha);
    // Outside DrawWorld's BeginMode2D/EndMode2D on purpose -- IconRenderer projects world space
    // to screen space itself, so its reticle stays a fixed pixel size under zoom instead of
    // scaling with the world like WorldRenderer's sprites do.
    render::DrawTargetReticle(world_.Registry(), camera, alpha);
}

void SpaceFlight::OnExit() {}

}  // namespace sr::space
