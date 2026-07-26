#include "modes/space/SpaceFlight.h"

#include "modes/space/systems/SystemSchedule.h"

namespace sr::space {

SpaceFlight::SpaceFlight(const core::ContentLibrary& content)
    : world_("sol", "Sol"), content_(content) {}

void SpaceFlight::OnEnter() {
    // Factories populate the world here once they land (first vertical slice, step 5-7):
    // WorldGen seeds the system, ShipFactory instantiates the player rig from its blueprint,
    // NpcFactory adds the opposition. Nothing is constructed inline in this file -- Law 5.
}

void SpaceFlight::Update(float realDeltaSeconds) {
    clock_.Advance(realDeltaSeconds);

    while (clock_.ConsumeStep()) {
        const SystemContext ctx{world_, intents_, content_, core::kFixedDeltaSeconds,
                                clock_.ElapsedTicks()};
        RunTick(ctx);
    }

    // Drained after the whole schedule, never mid-list: a system's view of this tick's input
    // must not depend on its position in the order.
    intents_.Clear();
}

void SpaceFlight::Draw(float alpha) const {
    (void)alpha;
    // render/ lands with the first vertical slice. Camera math belongs in this file (Law 7);
    // the draw calls themselves belong in modes/space/render/.
}

void SpaceFlight::OnExit() {}

}  // namespace sr::space
