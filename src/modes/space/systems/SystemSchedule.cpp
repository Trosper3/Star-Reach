#include "modes/space/systems/SystemSchedule.h"

#include "modes/space/systems/CollisionSystem.h"
#include "modes/space/systems/CommanderSystem.h"
#include "modes/space/systems/CommsSystem.h"
#include "modes/space/systems/ConstructionSystem.h"
#include "modes/space/systems/ContractSystem.h"
#include "modes/space/systems/DamageSystem.h"
#include "modes/space/systems/DiscoverySystem.h"
#include "modes/space/systems/DistressSystem.h"
#include "modes/space/systems/DockingSystem.h"
#include "modes/space/systems/EngineerSystem.h"
#include "modes/space/systems/FactionEconomySystem.h"
#include "modes/space/systems/HazardSystem.h"
#include "modes/space/systems/HierarchySystem.h"
#include "modes/space/systems/LootSystem.h"
#include "modes/space/systems/MiningSystem.h"
#include "modes/space/systems/ModuleEquipSystem.h"
#include "modes/space/systems/NpcAiSystem.h"
#include "modes/space/systems/OrbitSystem.h"
#include "modes/space/systems/PartySystem.h"
#include "modes/space/systems/PhysicsSystem.h"
#include "modes/space/systems/PowerSystem.h"
#include "modes/space/systems/ProjectileSystem.h"
#include "modes/space/systems/RefactorSystem.h"
#include "modes/space/systems/ResearchSystem.h"
#include "modes/space/systems/SpawnSystem.h"
#include "modes/space/systems/StationServicesSystem.h"
#include "modes/space/systems/TargetingSystem.h"
#include "modes/space/systems/TemplateMarketSystem.h"
#include "modes/space/systems/TutorialSystem.h"
#include "modes/space/systems/WarpSystem.h"
#include "modes/space/systems/WeaponSystem.h"

namespace sr::space {

// Add systems here as they land. Keep the ordering rules in SystemSchedule.h satisfied, and
// add the entry in the SAME commit as the system file -- a system with no schedule entry is
// a dead abstraction (architecture.md section 2.4) and will be deleted at review.
//
//   WarpSystem        -- runs first: a rig teleported this tick is repositioned before anything
//                        below reasons about position, including HierarchySystem and
//                        PhysicsSystem later in this same tick.
//   ConstructionSystem, ModuleEquipSystem -- before PowerSystem, so a freshly built rig or a
//                        mount/unmount lands in this same tick's power budget.
//   PowerSystem       -- recomputes PowerBudget.satisfaction from last tick's Destroyed
//                        tags, before anything that gates on it
//   SpawnSystem       -- settles a respawned rig's WorldTransform and culls far rigs before
//                        anything below reasons about position or distance this tick
//   OrbitSystem       -- sets planet/moon transforms and nudges ship Velocity via gravity
//                        wells before PhysicsSystem integrates it, same as thrust
//   PhysicsSystem     -- scales thrust by PowerBudget.satisfaction; moves every rig root to
//                        this tick's position.
//   HierarchySystem   -- after PhysicsSystem, not before. A hardpoint's WorldTransform is
//                        derived from its root's, and the root only reaches this tick's pose at
//                        PhysicsSystem -- running Hierarchy earlier (its position until this
//                        fix) left every hardpoint one tick stale for every downstream reader:
//                        collision hulls, hit tests, weapon aim (architecture.md 13.3 finding
//                        G). Before DockingSystem, so DockingSystem's range check reads each
//                        hardpoint's settled-this-tick position.
//   HazardSystem      -- after HierarchySystem (reads settled hardpoint WorldTransforms), before
//                        CollisionSystem/ProjectileSystem so a real attacker's PendingDamage
//                        overwrites the hazard's null-source one when both land the same tick.
//   DockingSystem     -- after PhysicsSystem/HierarchySystem so a freshly-Docked rig's
//                        Velocity/ThrustInput are zeroed, and its hardpoints already sit at
//                        their settled-this-tick position, before TargetingSystem/NpcAiSystem
//                        see it this same tick; removes Targetable the instant docking
//                        completes.
//   EngineerSystem, RefactorSystem -- after DockingSystem; both gate on Docked.
//   StationServicesSystem -- after DockingSystem, so a freshly docked rig can trade this tick.
//   TargetingSystem
//   NpcAiSystem       -- reads Target, writes FireIntent read by WeaponSystem this same tick.
//                        exclude<Docked> (architecture.md 13.3 finding H): a docked NPC must not
//                        thrust or ask to fire -- DockingSystem's Velocity/ThrustInput zeroing
//                        runs once, on dock, and an unexcluded NpcAiSystem would rewrite both
//                        every tick after.
//   WeaponSystem      -- gated by PowerSystem's budget once PowerSystem lands. exclude<Docked>
//                        for the same reason as NpcAiSystem: a docked rig, player or NPC, does
//                        not fire.
//   CollisionSystem   -- reads this tick's settled WorldTransform/Velocity; queues ramming
//                        PendingDamage the same as ProjectileSystem. exclude<Docked> on its
//                        ramming-candidate view: features.md 3.4's "a docked vessel is not a
//                        target," the exclusion half architecture.md 12.34 specifies.
//   ProjectileSystem  -- FindHit skips a hardpoint whose ParentRig carries Docked -- the same
//                        rule as CollisionSystem's exclusion (12.34), but not a view-level
//                        entt::exclude<Docked>: the tag lives on the rig root, and FindHit's
//                        view is over hardpoints, which never carry it.
//   PartySystem       -- after NpcAiSystem so formation ThrustInput sticks; before
//                        DamageSystem so PendingDamage is still readable for retaliation
//   DamageSystem      -- must be last of the combat systems; destruction is the tick's
//                        final word there. An Asteroid can be tagged Destroyed by this same
//                        generic drain, which is exactly what MiningSystem reacts to next.
//   TutorialSystem    -- after DamageSystem, before MiningSystem: its DestroyAsteroid step
//                        reads the same this-tick Destroyed tag MiningSystem is about to
//                        consume by destroying the entity outright.
//   MiningSystem      -- after DamageSystem so it sees this tick's Destroyed asteroids, and
//                        before LootSystem so an ElementDrop it spawns this tick is already
//                        visible to LootSystem's expiry/pickup pass the same tick.
//   ContractSystem    -- after DamageSystem for the same reason as MiningSystem: it credits
//                        Bounty kills off this tick's Destroyed rig roots. Independent of
//                        MiningSystem (Asteroid-tagged vs FactionRef-tagged Destroyed
//                        entities never overlap).
//   DistressSystem    -- after DamageSystem so a ShipUnderAttack call sees this tick's
//                        Destroyed tag on its npcTarget.
//   LootSystem        -- no ordering constraint against the above beyond that: it only reads
//                        this tick's settled WorldTransform/CollisionRadius and never spawns
//                        a drop of its own (Law 5 -- there is no LootFactory yet), so it runs
//                        last.
//   CommsSystem, FactionEconomySystem, DiscoverySystem, CommanderSystem, ResearchSystem,
//                        TemplateMarketSystem -- no ordering constraint among these or the
//                        above: each touches only its own state, or only core/ for the latter
//                        three.
//
const std::vector<ScheduledSystem>& TickSchedule() {
    static const std::vector<ScheduledSystem> schedule{
        {"WarpSystem", &warp_system::Tick},
        {"ConstructionSystem", &construction_system::Tick},
        {"ModuleEquipSystem", &module_equip_system::Tick},
        {"PowerSystem", &power_system::Tick},
        {"SpawnSystem", &spawn_system::Tick},
        {"OrbitSystem", &orbit_system::Tick},
        {"PhysicsSystem", &physics_system::Tick},
        {"HierarchySystem", &hierarchy_system::Tick},
        {"HazardSystem", &hazard_system::Tick},
        {"DockingSystem", &docking_system::Tick},
        {"EngineerSystem", &engineer_system::Tick},
        {"RefactorSystem", &refactor_system::Tick},
        {"StationServicesSystem", &station_services_system::Tick},
        {"TargetingSystem", &targeting_system::Tick},
        {"NpcAiSystem", &npc_ai_system::Tick},
        {"WeaponSystem", &weapon_system::Tick},
        {"CollisionSystem", &collision_system::Tick},
        {"ProjectileSystem", &projectile_system::Tick},
        {"PartySystem", &party_system::Tick},
        {"DamageSystem", &damage_system::Tick},
        {"TutorialSystem", &tutorial_system::Tick},
        {"MiningSystem", &mining_system::Tick},
        {"ContractSystem", &contract_system::Tick},
        {"DistressSystem", &distress_system::Tick},
        {"LootSystem", &loot_system::Tick},
        {"CommsSystem", &comms_system::Tick},
        {"FactionEconomySystem", &faction_economy_system::Tick},
        {"DiscoverySystem", &discovery_system::Tick},
        {"CommanderSystem", &commander_system::Tick},
        {"ResearchSystem", &research_system::Tick},
        {"TemplateMarketSystem", &template_market_system::Tick},
    };
    return schedule;
}

void RunTick(const SystemContext& ctx) {
    for (const ScheduledSystem& system : TickSchedule()) {
        system.tick(ctx);
    }
}

}  // namespace sr::space
