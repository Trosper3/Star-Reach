#include "modes/space/systems/SystemSchedule.h"

namespace sr::space {

const std::vector<ScheduledSystem>& TickSchedule() {
    // Add systems here as they land. Keep the ordering rules in SystemSchedule.h satisfied, and
    // add the entry in the SAME commit as the system file -- a system with no schedule entry is
    // a dead abstraction (architecture.md section 2.4) and will be deleted at review.
    //
    // The intended order once the first vertical slice is complete:
    //
    //   HierarchySystem   -- must be first; everything below reads WorldTransform
    //   PhysicsSystem
    //   TargetingSystem
    //   WeaponSystem      -- gated by PowerSystem's budget
    //   ProjectileSystem
    //   DamageSystem      -- must be last; destruction is the tick's final word
    //
    static const std::vector<ScheduledSystem> schedule{};
    return schedule;
}

void RunTick(const SystemContext& ctx) {
    for (const ScheduledSystem& system : TickSchedule()) {
        system.tick(ctx);
    }
}

}  // namespace sr::space
