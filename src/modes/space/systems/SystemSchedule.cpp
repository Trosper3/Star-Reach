#include "modes/space/systems/SystemSchedule.h"

#include "modes/space/systems/DamageSystem.h"
#include "modes/space/systems/HierarchySystem.h"
#include "modes/space/systems/PhysicsSystem.h"
#include "modes/space/systems/ProjectileSystem.h"
#include "modes/space/systems/TargetingSystem.h"
#include "modes/space/systems/WeaponSystem.h"

namespace sr::space {

const std::vector<ScheduledSystem>& TickSchedule() {
    // Add systems here as they land. Keep the ordering rules in SystemSchedule.h satisfied, and
    // add the entry in the SAME commit as the system file -- a system with no schedule entry is
    // a dead abstraction (architecture.md section 2.4) and will be deleted at review.
    //
    //   HierarchySystem   -- must be first; everything below reads WorldTransform
    //   PhysicsSystem
    //   TargetingSystem
    //   WeaponSystem      -- gated by PowerSystem's budget once PowerSystem lands
    //   ProjectileSystem
    //   DamageSystem      -- must be last; destruction is the tick's final word
    //
    static const std::vector<ScheduledSystem> schedule{
        {"HierarchySystem", &hierarchy_system::Tick},   {"PhysicsSystem", &physics_system::Tick},
        {"TargetingSystem", &targeting_system::Tick},   {"WeaponSystem", &weapon_system::Tick},
        {"ProjectileSystem", &projectile_system::Tick}, {"DamageSystem", &damage_system::Tick},
    };
    return schedule;
}

void RunTick(const SystemContext& ctx) {
    for (const ScheduledSystem& system : TickSchedule()) {
        system.tick(ctx);
    }
}

}  // namespace sr::space
