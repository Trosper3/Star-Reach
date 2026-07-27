#include "modes/space/systems/SystemSchedule.h"

#include "modes/space/systems/CollisionSystem.h"
#include "modes/space/systems/DamageSystem.h"
#include "modes/space/systems/HierarchySystem.h"
#include "modes/space/systems/LootSystem.h"
#include "modes/space/systems/NpcAiSystem.h"
#include "modes/space/systems/OrbitSystem.h"
#include "modes/space/systems/PartySystem.h"
#include "modes/space/systems/PhysicsSystem.h"
#include "modes/space/systems/PowerSystem.h"
#include "modes/space/systems/ProjectileSystem.h"
#include "modes/space/systems/SpawnSystem.h"
#include "modes/space/systems/TargetingSystem.h"
#include "modes/space/systems/WeaponSystem.h"

namespace sr::space {

const std::vector<ScheduledSystem>& TickSchedule() {
    // Add systems here as they land. Keep the ordering rules in SystemSchedule.h satisfied, and
    // add the entry in the SAME commit as the system file -- a system with no schedule entry is
    // a dead abstraction (architecture.md section 2.4) and will be deleted at review.
    //
    //   HierarchySystem   -- must be first; everything below reads WorldTransform
    //   PowerSystem       -- recomputes PowerBudget.satisfaction from last tick's Destroyed
    //                        tags, before anything that gates on it
    //   SpawnSystem       -- settles a respawned rig's WorldTransform and culls far rigs before
    //                        anything below reasons about position or distance this tick
    //   OrbitSystem       -- sets planet/moon transforms and nudges ship Velocity via gravity
    //                        wells before PhysicsSystem integrates it, same as thrust
    //   PhysicsSystem     -- scales thrust by PowerBudget.satisfaction
    //   TargetingSystem
    //   NpcAiSystem       -- reads Target, writes FireIntent read by WeaponSystem this same tick
    //   WeaponSystem      -- gated by PowerSystem's budget once PowerSystem lands
    //   CollisionSystem   -- reads this tick's settled WorldTransform/Velocity; queues ramming
    //                        PendingDamage the same as ProjectileSystem
    //   ProjectileSystem
    //   PartySystem       -- after NpcAiSystem so formation ThrustInput sticks; before
    //                        DamageSystem so PendingDamage is still readable for retaliation
    //   DamageSystem      -- must be last; destruction is the tick's final word
    //   LootSystem        -- no ordering constraint against the above: it only reads this
    //                        tick's settled WorldTransform/CollisionRadius and never spawns a
    //                        drop itself (Law 5 -- there is no LootFactory yet), so it runs last
    //
    static const std::vector<ScheduledSystem> schedule{
        {"HierarchySystem", &hierarchy_system::Tick},
        {"PowerSystem", &power_system::Tick},
        {"SpawnSystem", &spawn_system::Tick},
        {"OrbitSystem", &orbit_system::Tick},
        {"PhysicsSystem", &physics_system::Tick},
        {"TargetingSystem", &targeting_system::Tick},
        {"NpcAiSystem", &npc_ai_system::Tick},
        {"WeaponSystem", &weapon_system::Tick},
        {"CollisionSystem", &collision_system::Tick},
        {"ProjectileSystem", &projectile_system::Tick},
        {"PartySystem", &party_system::Tick},
        {"DamageSystem", &damage_system::Tick},
        {"LootSystem", &loot_system::Tick},
    };
    return schedule;
}

void RunTick(const SystemContext& ctx) {
    for (const ScheduledSystem& system : TickSchedule()) {
        system.tick(ctx);
    }
}

}  // namespace sr::space
