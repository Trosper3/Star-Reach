#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/diplomacy/RelationSeeding.h"
#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "core/time/FixedTimestep.h"
#include "engine/platform/Window.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/factories/RigFactory.h"
#include "modes/space/render/WorldRenderer.h"
#include "modes/space/systems/DamageSystem.h"
#include "modes/space/systems/HierarchySystem.h"
#include "modes/space/systems/NpcAiSystem.h"
#include "modes/space/systems/PhysicsSystem.h"
#include "modes/space/systems/PlayerInputSystem.h"
#include "modes/space/systems/PlayerLocationSystem.h"
#include "modes/space/systems/PowerSystem.h"
#include "modes/space/systems/ProjectileSystem.h"
#include "modes/space/systems/System.h"
#include "modes/space/systems/TargetingSystem.h"
#include "modes/space/systems/WeaponSystem.h"
#include "modes/space/ui/FlightControls.h"
#include "shared/blueprints/Ids.h"
#include "shared/blueprints/ShipBlueprint.h"
#include "shared/blueprints/Validation.h"
#include "shared/components/Combat.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"

// tools/sandbox -- the player-interaction sandbox: a small standalone executable that drives the
// REAL RigFactory/systems/render code against a private SystemWorld, so a feature under
// discussion can be tuned here and ported back by wiring the same call into SpaceFlight, rather
// than by re-deriving it from a description a second time. See MenuVignette.cpp for this
// codebase's existing precedent for the same idea (real systems, private world, no player save).
//
// Milestone 1: movement only. One real ship, spawned through RigFactory, driven by the real
// PlayerInputSystem/PhysicsSystem/HierarchySystem in the same relative order
// modes/space/systems/SystemSchedule.cpp's TickSchedule() runs them.
//
// Milestone 2: weapons/combat. A second, stationary rig (SpawnTargetDummy) gives the player's
// already-live FireIntent/AimPoint (FlightControls' LMB-fire and cursor-aim were already being
// polled and dropped in Milestone 1) something to hit. PowerSystem/TargetingSystem/WeaponSystem/
// ProjectileSystem/DamageSystem join TickSystems below in their real schedule order --
// CollisionSystem (ramming) and PartySystem/NpcAiSystem (formations, AI fire) are still out of
// scope with only one mobile rig in the world.
//
// Milestone 3: weapon variety + switching (MakeWeaponVarietyBlueprint). The player's
// wing_starboard mount trades its authored pulse_cannon_i for an autocannon_i -- a different
// ModuleId, so RigFactory::AttachModuleComponents lands it in a second WeaponGroup automatically
// (features.md 3.6). Switching is FlightControls' existing 1-0 SetWeaponGroupsIntent, already
// reaching PlayerInputSystem/EnabledWeaponGroups in Milestone 1 with nothing to toggle yet --
// group 0 (pulse cannon) is key 1, group 1 (autocannon) is key 2, no new input code required.
//
// Milestone 4: a wider roster (turret_flak/turret_railgun, groups 2/3 -- flak's 6-pellet spread
// and railgun's slow/heavy hit are the two real modules that read most differently from what was
// already there) plus a tier-slot experiment (wing_aux: two ModuleIds on one mount, see
// MakeWeaponVarietyBlueprint's own comment) probing the moduleSlots-bonus design features.md's
// rarity ladder and architecture.md 12.22 both describe but neither has built yet. Building
// wing_aux also surfaced a real production bug: RigFactory::AttachModule mis-assigned/double-
// emplaced WeaponGroup on any hardpoint carrying more than one module (fixed in the same commit --
// modes/space/factories/RigFactory.cpp).
//
// Milestone 5: continuous (beam), homing, chargeToFire (hold-to-charge, auto-fires at
// full charge -- there is deliberately no separate release-to-fire path), burst-fire, and
// configurable ammo -- all independent, composable Weapon behavior flags rather than an exclusive
// "weapon type" (shared/blueprints/ModuleDef.h's WeaponStats carries the full design). Five new
// mounts (groups 4-8, turret_beam/turret_tracking_beam/turret_seeker/turret_lockon/turret_burst)
// each isolate one mechanic, except turret_lockon, which deliberately combines homing + charge +
// finite ammo into the "true" lock-on missile this milestone was built to explore. A third
// reactor (reactor_aux2) joins reactor_aux for the same power-budget reason as Milestone 4's.
//
// Milestone 6: shields. Researching the real mechanic (Shield/ShieldCoverage/
// PowerShed in shared/components/Health.h and Power.h, DamageSystem's absorb-then-hull-damage
// split) surfaced a real production bug: RegenerateShield ignored both PowerShed and
// PowerBudget.shields entirely, so a browned-out or power-shed shield regenerated at full rate
// as if nothing had happened -- fixed in the same commit (DamageSystem.cpp), with regression
// coverage in tests/unit/DamageSystemTests.cpp. Three new shield mounts (shield_dissipator/
// shield_barrier/shield_bubble) join the pre-existing "emitter" (deflector_i, Conformal/kinetic)
// to put all three ShieldCoverage modes and both kinetic/energy absorbers on the player's own
// rig, so the existing weapon roster above has real, varied shields to fire at. Six purpose-built
// target dummies (MakeShieldDummyBlueprint) isolate one shield type each, covering the full 3
// (coverage: Personal/Conformal/Bubble) x 2 (absorbs: kinetic/energy) matrix of real
// shield_generator content -- which turned up one real gap along the way: the base game had no
// energy-absorbing Bubble shield, so bubble_projector_energy_i (data/base_game/modules.json) is
// new content added specifically to complete it, mirroring bubble_projector_i's own stats. A live
// hull/shield HUD readout per dummy (DrawShieldDummyHud) makes each one's state visible -- there
// is no in-game health bar anywhere in this project (Health.h's own comment) to watch otherwise.
//
// Watching the matrix get shot at surfaced the next real gap: nothing distinguished a shield
// absorbing a hit from an unshielded hardpoint taking one, in-world or in the HUD alike --
// features.md 3.5's own "in-world shield shimmer" is documented but explicitly flagged unbuilt
// (shared/ui/HudTheme.h's DamageTypeColor comment). ShieldImpactFlash (Health.h) and
// WorldRenderer::DrawShieldImpacts close that gap for the per-hit case: a fading ring, colored by
// the absorbed DamageType (DamageTypeColor -- same palette as projectiles/beams), positioned per
// coverage mode exactly as features.md 3.1/3.5 describe -- Personal and Conformal flash on the
// hit hardpoint itself, Bubble flashes out at its own coverageRadius perimeter. This is a real
// production feature (DamageSystem.cpp/WorldRenderer.cpp), not a sandbox-only effect -- it fires
// off the same PendingDamage/Shield absorption path production combat already runs, so shooting
// any of the six dummies above shows it live. The full persistent charge-dashed coverage loop
// features.md 3.5 also specifies remains unbuilt.
//
// Milestone 7 (this file): return fire. Every dummy up to now was passive -- Milestone 2's own
// comment named PartySystem/NpcAiSystem as explicitly out of scope "with only one mobile rig in
// the world." Wiring the real NpcAiSystem in (TickSystems below, between TargetingSystem and
// WeaponSystem, matching NpcAiSystem.h's own scheduling comment) surfaced two real gaps of its
// own: this file never emplaced PlayerLocation on the player's rig (SpawnPlayerShip), so
// NpcAiSystem's own exclude<PlayerLocation> would not have excluded the player from being driven
// by the dummy's AI too -- fixed by also running PlayerLocationSystem first, the same as
// production's own TickSchedule; and the target dummy's own faction (aegis_vanguard's authored
// aegis_directorate, data/base_game/ships.json) is the SAME faction the player's drafted blueprint
// uses, and DiplomacyMatrix::Get hard-codes a==b to Relation::Friendly regardless of matrix content
// -- so SpawnTargetDummy now overrides it to "reapers" (RigFactory::SpawnParams::faction),
// escalated to Relation::War against aegis_directorate specifically
// (core/diplomacy/RelationSeeding.cpp's SeedReaperHostility), and main() now builds and seeds a
// real DiplomacyMatrix instead of passing SystemContext::diplomacy as nullptr (TargetingSystem's
// own IsHostile fails closed on that). The dummy also had zero SensorRange (aegis_vanguard mounts
// no Sensor module) -- set by hand in SpawnTargetDummy rather than authoring a sensor mount onto
// real content just for this. The player's own Health/Shield (already proven against the six
// Milestone 6 shield dummies) can now actually be drained or destroyed by the dummy's fire; there
// is still no ramming (CollisionSystem remains out of TickSystems) and no formations/wingmen
// (PartySystem likewise).
//
// No menus beyond the existing debug readouts -- more gets added to TickSystems below, one call
// at a time, the day each is the thing under discussion.
namespace {

constexpr sr::ActorId kSandboxActorId{1};
// The target dummy stays authored content, unmodified -- only the player's loadout is under test.
constexpr const char* kDummyBlueprint = "aegis_vanguard";
constexpr const char* kPlayerBlueprint = "sandbox_weapon_variety";

// Same walk-up src/main.cpp's own FindContentDirectory does -- duplicated rather than shared,
// the same call MenuVignette.cpp's own small duplications make (not worth a shared header for
// one five-line walk).
std::filesystem::path FindContentDirectory() {
    std::filesystem::path dir = std::filesystem::current_path();
    for (int depth = 0; depth < 5; ++depth) {
        const std::filesystem::path candidate = dir / "data" / "base_game";
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
        if (!dir.has_parent_path() || dir.parent_path() == dir) {
            break;
        }
        dir = dir.parent_path();
    }
    return {};
}

std::optional<sr::core::ContentLibrary> LoadContent(const std::filesystem::path& dir) {
    sr::core::ContentLibrary content;
    const sr::core::LoadReport load = content.LoadFromDirectory(dir);
    if (!load.ok()) {
        std::cerr << "sandbox: " << load.Summary() << "\n";
        return std::nullopt;
    }
    const sr::core::LoadReport validation = content.ValidateAll();
    if (!validation.ok()) {
        std::cerr << "sandbox: " << validation.Summary() << "\n";
        return std::nullopt;
    }
    return content;
}

// Hand-builds a ShipBlueprint that starts from aegis_vanguard (data/base_game/ships.json) and
// widens its weapon roster to four distinct types plus a tier-slot experiment. RigFactory only
// ever asks ContentLibrary::FindShip to resolve a BlueprintId, and FindShip checks the runtime
// "drafted" overlay (craftedShips_) before the authored set (ContentLibrary.cpp) -- the same
// overlay ConstructionSystem::RegisterDraftedTemplate writes a player's saved Template into
// (architecture.md 12.30.8). Registering one here goes through that exact real lookup path --
// RigFactory, AttachModuleComponents, WeaponGroup assignment all run identically to authored
// content -- rather than inventing a sandbox-only spawn shortcut. This is legitimate specifically
// because tools/ is one of the directories tools/ci/check_content_pipeline.py exempts from Law
// 10's "no plain ShipBlueprint initializer outside core/registries/" rule (ContentLibrary.h's
// comment on RegisterCraftedModule explains why the exemption exists): everywhere else, this
// struct may only come from JSON.
//
// wing_aux is the interesting one: two ModuleIds on one shell_wing_hardpoint mount
// (pulse_cannon_i + fire_control_i), testing features.md's documented-but-unbuilt "rarity ladder"
// design (settled 2026-08-07: higher-tier shells grant bonus moduleSlots) and architecture.md
// 12.22's "a 2-slot turret fits weapon plus fire control while a 1-slot one does not" -- neither
// is implemented (moduleSlots is a flat 1 on every real weapon shell today), but RigFactory
// itself never enforces a shell's moduleSlots count at spawn time -- only
// shared/blueprints/Validation.cpp's MountCapacity rule does, at content-authoring time, which
// this drafted blueprint never runs through (LoadContent's ValidateAll already ran once, over the
// authored set, before this function is even called). So a two-module mount is not a sandbox
// shortcut here either: it is real content (fire_control_i already exists, data/base_game/
// modules.json) run through the real attach path (ModuleAttachment.cpp's FireControl case), just
// without the not-yet-built moduleSlots gate that would otherwise require it.
sr::ShipBlueprint MakeWeaponVarietyBlueprint() {
    sr::ShipBlueprint blueprint;
    blueprint.id = sr::BlueprintId(kPlayerBlueprint);
    blueprint.displayName = "Sandbox Weapon Variety";
    blueprint.faction = sr::FactionId("aegis_directorate");
    blueprint.mobile = true;
    // Well above aegis_vanguard's authored 260: nine extra weapon-ish mounts, two extra
    // reactors, and (Milestone 6) three extra shield emitters would exceed it, and this
    // blueprint exists to explore weapon/shield variety, not to stay inside a real hull's
    // structural budget.
    blueprint.structuralMassLimit = 760.0f;

    const auto addMount = [&](const char* id, const char* shell, const char* attachedTo,
                              std::vector<sr::ModuleId> modules, sr::Vec2 localOffset,
                              float traverseRadians = 0.0f) {
        sr::MountBlueprint mount;
        mount.id = sr::MountId(id);
        mount.shell = sr::ShellId(shell);
        mount.modules = std::move(modules);
        mount.localOffset = localOffset;
        mount.attachedTo = sr::MountId(attachedTo);
        mount.traverseRadians = traverseRadians;
        blueprint.rig.mounts.push_back(std::move(mount));
    };

    addMount("core", "shell_fighter_chassis", "", {sr::ModuleId("armor_plate_i")}, {0.0f, 0.0f});
    addMount("reactor", "shell_power_bay", "core", {sr::ModuleId("power_cell_i")},
             {-8.5f, -14.7224f});
    // Two extra reactors: nine weapon-ish mounts below draw far more than one power_cell_i's 140
    // generation covers, and PowerSystem's real brownout/shed behavior would otherwise obscure
    // "does this weapon type work" behind "is this rig starved of power" -- a different, real
    // system (already exercised by the sandbox), just not the one under discussion right now.
    // Both live on hubs (below), same as every other new mount -- see the hub comment for why.
    addMount("thruster_main", "shell_thruster_mount", "core", {sr::ModuleId("ion_thruster_i")},
             {-18.0f, 0.0f});
    addMount("wing_port", "shell_wing_hardpoint", "core", {sr::ModuleId("pulse_cannon_i")},
             {8.5f, -14.7224f}, 0.35f);
    addMount("wing_starboard", "shell_wing_hardpoint", "core", {sr::ModuleId("autocannon_i")},
             {8.5f, 14.7224f}, 0.35f);

    // Structural hub mounts (shell_bridge, an armor-kind shell -- no module of its own, purely a
    // structural extension). shared/blueprints/Validation.cpp's CheckAttachment rule (Rule 11)
    // requires every mount to stay within shell.radius + parent.radius of its OWN parent -- for a
    // mount attached straight to "core" (shell_fighter_chassis, radius 20), that is a hard 25-unit
    // ceiling from the origin for a radius-5 weapon mount. Nine more weapon-ish mounts plus two
    // reactors cannot fit inside that disk alongside aegis_vanguard's original six without
    // violating CheckSeparation's pairwise-disjoint rule (Rule 10) instead -- verified by brute-
    // force search, not by eye: fewer than the nine survive before the search space is exhausted.
    // Three hubs attached to core (themselves within ITS 30-unit budget, radius 10 + core's 20)
    // each open their own 15-unit attachment budget (radius 5 + hub's 10) for whatever attaches
    // to THEM instead of to core, which is what makes the rest of the roster fit. This is exactly
    // what happened building wing_aux earlier
    // (Milestone 4): Validate() runs on every Spawn(), authored or drafted, and a blueprint that
    // fails it spawns nothing -- silently, since neither SpawnPlayerShip nor SpawnTargetDummy
    // printed on failure until this was added. All coordinates below are copied verbatim from a
    // verified layout (0 Separation/Attachment violations), not hand-placed.
    addMount("hub_a", "shell_bridge", "core", {}, {27.154893f, 11.526568f});
    addMount("hub_b", "shell_bridge", "core", {}, {-25.675493f, -14.526495f});
    addMount("hub_c", "shell_bridge", "core", {}, {-3.595146f, -29.280111f});

    addMount("reactor_aux", "shell_power_bay", "hub_a", {sr::ModuleId("power_cell_i")},
             {41.854893f, 11.526568f});
    addMount("reactor_aux2", "shell_power_bay", "hub_b", {sr::ModuleId("power_cell_i")},
             {-29.356079f, -0.294725f});
    // Group 2: 6-pellet spread per shot -- visually the most distinct of the roster.
    addMount("turret_flak", "shell_wing_hardpoint", "hub_c", {sr::ModuleId("flak_battery_i")},
             {11.104854f, -29.280111f}, 0.35f);
    // Group 3: slow, heavy, long-ranged -- the opposite end of the roster from flak.
    addMount("turret_railgun", "shell_wing_hardpoint", "hub_a", {sr::ModuleId("railgun_i")},
             {38.164542f, 21.267083f}, 0.35f);
    // The tier-slot experiment (see function comment): shell_wing_hardpoint_ii is a real 2-slot
    // shell (data/base_game/shells.json), not a validation-skipping shortcut like the 1-slot
    // version this used before Milestone 5 -- MountCapacity (Rule "not enumerated... but implied
    // by moduleSlots") rejects 2 modules on a 1-slot shell exactly as strictly as any other rule
    // here, authored content included. Same ModuleId as wing_port, so it shares WeaponGroup 0
    // (features.md 3.6's per-ModuleId grouping) -- toggling key 1 fires both, which is exactly
    // what makes this an apples-to-apples comparison of the SAME gun's turret-traverse speed with
    // and without fire_control_i's boost, rather than a different weapon entirely.
    addMount("wing_aux", "shell_wing_hardpoint_ii", "hub_b",
             {sr::ModuleId("pulse_cannon_i"), sr::ModuleId("fire_control_i")},
             {-37.862345f, -6.306359f}, 0.35f);
    // Milestone 5: continuous, homing, chargeToFire/lock-on, and burst -- WeaponStats' own
    // comment (shared/blueprints/ModuleDef.h) has the full design; each mount below isolates one
    // (or, for the lock-on missile, the one deliberately-combined case: homing + chargeToFire +
    // maxAmmo) so its behavior reads clearly rather than several new mechanics blurring together
    // on one hardpoint. Wide traverseRadians (1.2 rad, vs the direct-fire guns' 0.35): missile/
    // beam racks are conventionally more omnidirectional than a fixed-forward cannon.
    //
    // Group 4: plain continuous beam, aimed at the cursor exactly like every other weapon --
    // features.md 3.2's manual aim is untouched, only the fire mechanic (per-tick hit, no
    // Projectile) differs.
    addMount("turret_beam", "shell_wing_hardpoint", "hub_c", {sr::ModuleId("beam_lance_i")},
             {-18.239208f, -27.998922f}, 1.2f);
    // Group 5: continuous + homing with no charge -- the beam+homing combination that looked like
    // a conflict earlier in design (a beam has no travel time to curve over) resolves by
    // redirecting the beam's live aim point to the nearest eligible target every tick instead of
    // steering a trajectory. No lock needed; it simply re-seeks continuously.
    addMount("turret_tracking_beam", "shell_wing_hardpoint", "hub_a",
             {sr::ModuleId("tracking_beam_i")}, {28.946373f, 26.116997f}, 1.2f);
    // Group 6: dumb-fire homing -- fires normally at the cursor like any other weapon (no lock,
    // no charge), then ProjectileSystem curves it toward the nearest eligible target after launch.
    addMount("turret_seeker", "shell_wing_hardpoint", "hub_b", {sr::ModuleId("seeker_missile_i")},
             {-40.249732f, -16.445230f}, 1.2f);
    // Group 7: the full lock-on -- press must land on a target (WeaponSystem's lock-acquisition
    // step), hold to charge (auto-fires at full charge, no separate release-to-fire path -- see
    // WeaponStats::chargeToFire's comment), and a finite 6-shot rack (maxAmmo) to exercise ammo
    // depletion. Combines three independent flags deliberately, unlike the other four mounts
    // here, to prove they compose rather than only working in isolation.
    addMount("turret_lockon", "shell_wing_hardpoint", "hub_c", {sr::ModuleId("lockon_missile_i")},
             {-15.411841f, -38.024006f}, 1.2f);
    // Group 8: burst fire -- three shots 0.08s apart per trigger pull, distinct from flak's
    // simultaneous pellet fan (projectilesPerShot).
    addMount("turret_burst", "shell_wing_hardpoint", "hub_a", {sr::ModuleId("chain_burst_i")},
             {18.828721f, 23.641223f}, 0.35f);
    addMount("emitter", "shell_shield_emitter", "core", {sr::ModuleId("deflector_i")},
             {17.0f, 0.0f});
    // Milestone 6: shields. "emitter" (deflector_i, above) predates this milestone and was
    // already Conformal/kinetic; these three isolate the rest of the real shield content
    // (data/base_game/modules.json) so all three ShieldCoverage modes and both damage-type
    // absorbers are on the same rig to fire the existing weapon roster at. Coordinates verified
    // by the same brute-force search as the hub mounts above -- each attaches directly to an
    // existing hub/core rather than needing a new one, since a shell_shield_emitter mount (radius
    // 5) is small enough to still fit that hub's remaining budget even fully loaded with
    // turrets/reactors.
    //
    // dissipator_i: energy-absorbing, Conformal coverage (wraps the whole rig).
    addMount("shield_dissipator", "shell_shield_emitter", "core", {sr::ModuleId("dissipator_i")},
             {18.711855f, -16.123166f});
    // barrier_kinetic_i: kinetic-absorbing, Personal coverage (protects only its own hardpoint).
    addMount("shield_barrier", "shell_shield_emitter", "hub_b", {sr::ModuleId("barrier_kinetic_i")},
             {-35.367876f, -25.578540f});
    // bubble_projector_i: kinetic-absorbing, Bubble coverage (protects hardpoints within
    // coverageRadius of it, not just its own rig position).
    addMount("shield_bubble", "shell_shield_emitter", "hub_c", {sr::ModuleId("bubble_projector_i")},
             {-6.714158f, -43.645408f});
    addMount("hold", "shell_facility_bay", "core", {sr::ModuleId("cargo_bay_i")},
             {-13.0f, 22.5167f});
    addMount("cockpit", "shell_cockpit", "hold", {sr::ModuleId("crew_officer_i")},
             {-18.0f, 31.177f});

    return blueprint;
}

// Milestone 6's target dummies: a minimal blueprint (chassis + reactor + at most one shield
// generator) rather than another aegis_vanguard variant -- isolating exactly one shield type (or
// none, as a baseline) per hull is what makes "did THIS shield's coverage mode/absorbed type
// actually change what got through" independently legible, the same reason Milestone 5 gave each
// weapon behavior its own turret instead of bolting all of them onto one rig. `shieldModuleId ==
// nullptr` builds the baseline: no Shield component anywhere on the rig, so DamageSystem's
// FindCoveringShield always resolves entt::null and every hit lands straight on Health.
// `mobile = false` (no thruster mount here) is required by Validation.cpp's own rule tying
// ShipBlueprint::mobile to needing an Engine-kind shell -- these dummies never move, matching
// SpawnTargetDummy's existing stationary aegis_vanguard.
sr::ShipBlueprint MakeShieldDummyBlueprint(const char* blueprintId, const char* shieldModuleId) {
    sr::ShipBlueprint blueprint;
    blueprint.id = sr::BlueprintId(blueprintId);
    blueprint.displayName = blueprintId;
    blueprint.faction = sr::FactionId("aegis_directorate");
    blueprint.mobile = false;
    // chassis(40)+armor(18)+power_bay(15)+power_cell(22) = 95 baseline, +10+30 for the heaviest
    // shield (bubble_projector_i) = 135 at most -- 200 leaves comfortable headroom.
    blueprint.structuralMassLimit = 200.0f;

    const auto addMount = [&](const char* id, const char* shell, const char* attachedTo,
                              std::vector<sr::ModuleId> modules, sr::Vec2 localOffset) {
        sr::MountBlueprint mount;
        mount.id = sr::MountId(id);
        mount.shell = sr::ShellId(shell);
        mount.modules = std::move(modules);
        mount.localOffset = localOffset;
        mount.attachedTo = sr::MountId(attachedTo);
        blueprint.rig.mounts.push_back(std::move(mount));
    };

    // Both satellites sit well inside core's 25-unit attachment budget (radius 5 + core's 20) and
    // are 30 units apart from each other, clear of their 10-unit (5+5) separation requirement --
    // simple enough to place by hand, unlike the crowded player rig above.
    addMount("core", "shell_fighter_chassis", "", {sr::ModuleId("armor_plate_i")}, {0.0f, 0.0f});
    addMount("reactor", "shell_power_bay", "core", {sr::ModuleId("power_cell_i")}, {-15.0f, 0.0f});
    if (shieldModuleId != nullptr) {
        addMount("shield", "shell_shield_emitter", "core", {sr::ModuleId(shieldModuleId)},
                 {15.0f, 0.0f});
    }

    return blueprint;
}

// Prints every reason a blueprint failed real validation. rig_factory::Spawn calls
// shared/blueprints/Validation.cpp's Validate() on EVERY spawn, authored content or a drafted
// blueprint alike -- SpawnResult::ok() only reports pass/fail, not why, which is what let this
// sandbox spawn nothing for two milestones (Milestone 4's wing_aux, then Milestone 5's wider
// mount layout) before anyone noticed: neither SpawnPlayerShip nor SpawnTargetDummy printed on
// failure. See MakeWeaponVarietyBlueprint's hub comment for what was actually wrong.
void ReportSpawnFailure(const sr::core::ContentLibrary& content,
                        const sr::BlueprintId& blueprintId) {
    const sr::ShipBlueprint* blueprint = content.FindShip(blueprintId);
    if (blueprint == nullptr) {
        std::cerr << "sandbox: blueprint '" << blueprintId.str() << "' not found\n";
        return;
    }
    const sr::ValidationResult result = sr::Validate(*blueprint, content);
    std::cerr << "sandbox: blueprint '" << blueprintId.str() << "' failed validation ("
              << result.errors.size() << " error(s)):\n";
    for (const sr::ValidationError& error : result.errors) {
        std::cerr << "  [" << sr::ToString(error.rule) << "] " << error.message << "\n";
    }
}

// Spawns the one real ship this milestone flies, tagged with the same ActorRef FlightControls'
// intents and PlayerInputSystem's resolution already agree on.
entt::entity SpawnPlayerShip(sr::space::SystemWorld& world,
                             const sr::core::ContentLibrary& content) {
    sr::space::rig_factory::SpawnParams params;
    params.blueprint = sr::BlueprintId(kPlayerBlueprint);
    params.position = sr::Vec2{0.0f, 0.0f};
    params.rotation = 0.0f;

    const sr::space::rig_factory::SpawnResult result =
        sr::space::rig_factory::Spawn(world, content, params);
    if (result.ok()) {
        world.Registry().emplace<sr::ActorRef>(result.root, sr::ActorRef{kSandboxActorId});
        // Milestone 7: without this, PlayerLocationSystem (not yet run here either -- see
        // TickSystems) would never derive PlayerControlled onto this rig, and NpcAiSystem's own
        // exclude<PlayerLocation> (NpcAiSystem.cpp) would not exclude it either -- the target
        // dummy's AI would try to fly and fire THIS rig too, fighting PlayerInputSystem for
        // ThrustInput/FireIntent on the same tick (architecture.md 12.30.1).
        world.Registry().emplace<sr::PlayerLocation>(result.root, sr::PlayerLocation{result.root});
    } else {
        ReportSpawnFailure(content, params.blueprint);
    }
    return result.root;
}

// Shared by SpawnTargetDummy and the Milestone 6 shield dummies below: no ActorRef, so
// PlayerInputSystem never touches it and it simply sits still -- WeaponSystem/ProjectileSystem/
// DamageSystem don't gate on FactionRef at all (only TargetingSystem's hostile-acquisition does,
// which the player's own AimPoint-driven fire bypasses per WeaponSystem::AimPointPosition), so a
// stationary target needs no faction wrangling.
// `faction` overrides the blueprint's authored faction when non-empty (RigFactory::SpawnParams'
// own comment) -- every caller except SpawnTargetDummy below leaves it default (empty), keeping
// aegis_directorate, so the six shield dummies stay the same non-hostile faction as the player and
// are never touched by NpcAiSystem/TargetingSystem's hostile-acquisition path.
entt::entity SpawnStationaryDummy(sr::space::SystemWorld& world,
                                  const sr::core::ContentLibrary& content,
                                  const sr::BlueprintId& blueprintId, sr::Vec2 position,
                                  sr::FactionId faction = sr::FactionId()) {
    sr::space::rig_factory::SpawnParams params;
    params.blueprint = blueprintId;
    params.faction = std::move(faction);
    params.position = position;
    params.rotation = 3.14159265f;
    const sr::space::rig_factory::SpawnResult result =
        sr::space::rig_factory::Spawn(world, content, params);
    if (!result.ok()) {
        ReportSpawnFailure(content, params.blueprint);
    }
    return result.root;
}

// Ahead of the player's spawn point, within pulse_cannon_i's 750-unit rangeUnits (data/base_game/
// modules.json) without any flying required first.
//
// Milestone 7: return fire. Spawned under "reapers" rather than aegis_vanguard's own authored
// faction (data/base_game/ships.json) -- the player's own drafted blueprint uses aegis_directorate
// too (MakeWeaponVarietyBlueprint), and DiplomacyMatrix::Get hard-codes a==b to Relation::Friendly
// regardless of matrix content (DiplomacyMatrix.cpp), so this dummy could never read as hostile
// without a real faction override. "reapers" is escalated all the way to Relation::War against
// aegis_directorate specifically (RelationSeeding.cpp's SeedReaperHostility) -- the strongest,
// least ambiguous hostile pairing seeded anywhere in the base game.
entt::entity SpawnTargetDummy(sr::space::SystemWorld& world,
                              const sr::core::ContentLibrary& content) {
    const entt::entity dummy =
        SpawnStationaryDummy(world, content, sr::BlueprintId(kDummyBlueprint),
                             sr::Vec2{300.0f, 0.0f}, sr::FactionId("reapers"));
    if (dummy != entt::null) {
        // aegis_vanguard mounts no Sensor module, so RigFactory aggregates its SensorRange to
        // 0 -- TargetingSystem::AcquireNearestHostile uses that raw value directly as its search
        // radius (TargetingSystem.cpp), so an unmodified dummy would never see anything to shoot
        // at. Set by hand here rather than authoring a sensor mount onto real content just to
        // make the sandbox's own dummy see the player.
        world.Registry().get<sr::SensorRange>(dummy).units = 2000.0f;
    }
    return dummy;
}

// One entry per Milestone 6 shield dummy: which shield module (if any) it carries, for the HUD
// label, plus the spawned hardpoint handles DrawShieldDummyHud reads from every frame.
struct ShieldDummySpec {
    const char* blueprintId;
    const char* shieldModuleId;  // nullptr for the no-shield baseline.
    const char* label;
    sr::Vec2 position;
};

struct ShieldDummy {
    const char* label;
    entt::entity root = entt::null;
    entt::entity core = entt::null;
    entt::entity shield = entt::null;  // entt::null if this dummy has no shield module.
};

// Full 3 (coverage) x 2 (absorbs) matrix -- every real shield_generator archetype in
// data/base_game/modules.json gets its own dummy. bubble_projector_energy_i is new content added
// alongside this milestone specifically to fill the matrix's one real gap: the base game had
// Bubble coverage only as kinetic (bubble_projector_i/ii) before this.
constexpr ShieldDummySpec kShieldDummySpecs[] = {
    {"sandbox_shield_personal_kinetic",
     "barrier_kinetic_i",
     "personal / kinetic (barrier_kinetic_i)",
     {650.0f, -500.0f}},
    {"sandbox_shield_personal_energy",
     "barrier_energy_i",
     "personal / energy (barrier_energy_i)",
     {650.0f, -300.0f}},
    {"sandbox_shield_conformal_kinetic",
     "deflector_i",
     "conformal / kinetic (deflector_i)",
     {650.0f, -100.0f}},
    {"sandbox_shield_conformal_energy",
     "dissipator_i",
     "conformal / energy (dissipator_i)",
     {650.0f, 100.0f}},
    {"sandbox_shield_bubble_kinetic",
     "bubble_projector_i",
     "bubble / kinetic (bubble_projector_i)",
     {650.0f, 300.0f}},
    {"sandbox_shield_bubble_energy",
     "bubble_projector_energy_i",
     "bubble / energy (bubble_projector_energy_i)",
     {650.0f, 500.0f}},
};

// Registers and spawns every Milestone 6 shield dummy. Drafted templates the same way
// MakeWeaponVarietyBlueprint's player rig is (content->RegisterDraftedTemplate), so each dummy
// spawns through the identical real RigFactory/Validate path as authored content.
std::vector<ShieldDummy> SpawnShieldDummies(sr::space::SystemWorld& world,
                                            sr::core::ContentLibrary& content) {
    std::vector<ShieldDummy> dummies;
    dummies.reserve(std::size(kShieldDummySpecs));
    for (const ShieldDummySpec& spec : kShieldDummySpecs) {
        content.RegisterDraftedTemplate(
            MakeShieldDummyBlueprint(spec.blueprintId, spec.shieldModuleId));
        ShieldDummy dummy;
        dummy.label = spec.label;
        dummy.root =
            SpawnStationaryDummy(world, content, sr::BlueprintId(spec.blueprintId), spec.position);
        if (dummy.root != entt::null) {
            dummy.core = sr::space::rig_factory::FindHardpoint(world.Registry(), dummy.root,
                                                               sr::MountId("core"));
            if (spec.shieldModuleId != nullptr) {
                dummy.shield = sr::space::rig_factory::FindHardpoint(world.Registry(), dummy.root,
                                                                     sr::MountId("shield"));
            }
        }
        dummies.push_back(dummy);
    }
    return dummies;
}

// Debug readout, sandbox-only: per-dummy hull (Health) and shield (Shield) state, read straight
// off the real components DamageSystem itself mutates -- there is no in-game health bar anywhere
// in this project to watch otherwise (Health.h's own comment). `absorbs`/`coverage` are read live
// off the Shield component (sr::ToString) rather than hardcoded from the spec, so this stays
// truthful even if a module's stats change.
void DrawShieldDummyHud(const entt::registry& registry, const std::vector<ShieldDummy>& dummies,
                        int startY) {
    int y = startY;
    for (const ShieldDummy& dummy : dummies) {
        if (dummy.core == entt::null) {
            DrawText(TextFormat("%s: (failed to spawn)", dummy.label), 12, y, 16, RED);
            y += 20;
            continue;
        }
        const auto& health = registry.get<sr::Health>(dummy.core);
        const bool rigDestroyed = registry.all_of<sr::Destroyed>(dummy.root);
        std::string shieldText = "none";
        if (dummy.shield != entt::null) {
            if (registry.all_of<sr::Destroyed>(dummy.shield)) {
                shieldText = "DESTROYED";
            } else {
                const auto& shield = registry.get<sr::Shield>(dummy.shield);
                shieldText = TextFormat("%.0f/%.0f (%s/%s)", shield.current, shield.max,
                                        std::string(sr::ToString(shield.coverage)).c_str(),
                                        std::string(sr::ToString(shield.absorbs)).c_str());
            }
        }
        DrawText(TextFormat("%s -- hull %.0f/%.0f  shield %s%s", dummy.label, health.current,
                            health.max, shieldText.c_str(), rigDestroyed ? "  [DESTROYED]" : ""),
                 12, y, 16, rigDestroyed ? RED : RAYWHITE);
        y += 20;
    }
}

// Draw-only parallax backdrop so movement reads against something -- entirely presentation, no
// gameplay state, so it lives here rather than in modes/space/render/ (Law 7). Nine layers,
// slowest/dimmest (most distant) to fastest/brightest (nearest): 1px faint, 1px medium, 1px
// bright, 2px faint, 2px medium, 2px bright, 3px faint, 3px medium, 3px bright. `parallax` is
// the fraction of camera movement each layer tracks -- near 0 barely moves (far background),
// near 1 tracks the camera almost fully (near foreground).
struct Star {
    sr::Vec2 tilePos;
};

struct StarLayer {
    float parallax;
    int pixelSize;
    Color color;
    std::vector<Star> stars;
};

// Larger than any realistic monitor's diagonal (Window::Open maximizes to fill it) so a star's
// wrap-around tiling never visibly seams on screen.
constexpr float kStarTileUnits = 4200.0f;

std::vector<StarLayer> MakeStarfield() {
    struct LayerSpec {
        float parallax;
        int pixelSize;
        Color color;
        int count;
    };
    constexpr Color kFaint{140, 150, 170, 130};
    constexpr Color kMedium{185, 195, 215, 180};
    constexpr Color kBright{225, 235, 255, 230};
    // Small/faint = slowest (farthest); large/bright = fastest (nearest) -- both size and
    // brightness climb together, in step, across the nine layers.
    const LayerSpec specs[] = {
        {0.010f, 1, kFaint, 240}, {0.020f, 1, kMedium, 200}, {0.035f, 1, kBright, 160},
        {0.060f, 2, kFaint, 130}, {0.090f, 2, kMedium, 100}, {0.130f, 2, kBright, 80},
        {0.170f, 3, kFaint, 60},  {0.210f, 3, kMedium, 45},  {0.260f, 3, kBright, 32},
    };

    std::vector<StarLayer> layers;
    layers.reserve(std::size(specs));
    for (const LayerSpec& spec : specs) {
        StarLayer layer{spec.parallax, spec.pixelSize, spec.color, {}};
        layer.stars.reserve(spec.count);
        for (int i = 0; i < spec.count; ++i) {
            layer.stars.push_back(Star{
                sr::Vec2{static_cast<float>(GetRandomValue(0, static_cast<int>(kStarTileUnits))),
                         static_cast<float>(GetRandomValue(0, static_cast<int>(kStarTileUnits)))}});
        }
        layers.push_back(std::move(layer));
    }
    return layers;
}

// Wraps `value` (a star or nebula's tile-space coordinate minus the parallax-scaled camera
// offset) into (-tileUnits/2, tileUnits/2] so each layer tiles seamlessly as the camera moves.
float WrapToTile(float value, float tileUnits) {
    const float half = tileUnits * 0.5f;
    value = std::fmod(value + half, tileUnits);
    if (value < 0.0f) {
        value += tileUnits;
    }
    return value - half;
}

// Distant colored gas clouds, layered behind every star for the same "depth via parallax" reason
// -- entirely presentation, same as StarLayer above. Both layers move slower than the slowest
// star layer (0.010): nebulae read as the farthest thing in the sky, farther than any star.
struct Nebula {
    sr::Vec2 tilePos;
    float radius;
    Color color;
};

struct NebulaLayer {
    float parallax;
    std::vector<Nebula> blobs;
};

// Bigger than the star tile: nebulae are sparse and individually huge, so a small tile would
// make the same handful of blobs repeat often enough to read as a grid.
constexpr float kNebulaTileUnits = 9000.0f;

std::vector<NebulaLayer> MakeNebulae() {
    struct LayerSpec {
        float parallax;
        int count;
        float minRadius;
        float maxRadius;
    };
    const LayerSpec specs[] = {
        {0.003f, 4, 500.0f, 900.0f},
        {0.006f, 5, 260.0f, 520.0f},
    };
    constexpr Color kPalette[] = {
        {90, 40, 120, 255},
        {30, 70, 130, 255},
        {130, 40, 70, 255},
        {30, 110, 100, 255},
    };

    std::vector<NebulaLayer> layers;
    layers.reserve(std::size(specs));
    for (const LayerSpec& spec : specs) {
        NebulaLayer layer{spec.parallax, {}};
        layer.blobs.reserve(spec.count);
        for (int i = 0; i < spec.count; ++i) {
            const float radius = static_cast<float>(
                GetRandomValue(static_cast<int>(spec.minRadius), static_cast<int>(spec.maxRadius)));
            const Color tint =
                kPalette[GetRandomValue(0, static_cast<int>(std::size(kPalette)) - 1)];
            layer.blobs.push_back(Nebula{
                sr::Vec2{static_cast<float>(GetRandomValue(0, static_cast<int>(kNebulaTileUnits))),
                         static_cast<float>(GetRandomValue(0, static_cast<int>(kNebulaTileUnits)))},
                radius, tint});
        }
        layers.push_back(std::move(layer));
    }
    return layers;
}

// Fakes a soft glow with no texture asset. There is no existing nebula rendering anywhere in
// src/ to match against (checked MainMenu.cpp/MenuVignette.cpp -- the main menu's background is
// a starfield only, no nebula coloration), so this is a from-scratch replacement for the earlier
// nested-rings version, which drew as visibly discrete bands once BLEND_ADDITIVE stacked them.
// raylib's own gradient-fill circle interpolates color per pixel across the whole disc in one
// draw call -- genuinely continuous, not banded -- so it replaces the manual ring loop outright.
void DrawNebulaGlow(sr::Vec2 center, float radius, Color color) {
    constexpr unsigned char kCoreAlpha = 70;
    Color core = color;
    core.a = kCoreAlpha;
    Color edge = color;
    edge.a = 0;
    DrawCircleGradient(static_cast<int>(center.x), static_cast<int>(center.y), radius, core, edge);
}

// BLEND_ADDITIVE so overlapping nebulae brighten toward white the way real emission clouds do,
// instead of muddying into a flat average under normal alpha blending.
void DrawNebulae(const std::vector<NebulaLayer>& layers, sr::Vec2 cameraTarget) {
    const float screenCenterX = static_cast<float>(GetScreenWidth()) * 0.5f;
    const float screenCenterY = static_cast<float>(GetScreenHeight()) * 0.5f;
    BeginBlendMode(BLEND_ADDITIVE);
    for (const NebulaLayer& layer : layers) {
        const float offsetX = cameraTarget.x * layer.parallax;
        const float offsetY = cameraTarget.y * layer.parallax;
        for (const Nebula& blob : layer.blobs) {
            const float x = WrapToTile(blob.tilePos.x - offsetX, kNebulaTileUnits) + screenCenterX;
            const float y = WrapToTile(blob.tilePos.y - offsetY, kNebulaTileUnits) + screenCenterY;
            DrawNebulaGlow(sr::Vec2{x, y}, blob.radius, blob.color);
        }
    }
    EndBlendMode();
}

// Splats an NxN star as a coverage-weighted blend across every pixel its floating-point
// [x, x+size) x [y, y+size) square partially overlaps, instead of snapping to
// static_cast<int>(x), static_cast<int>(y) -- a hard snap means a star does not move at all
// until enough sub-pixel drift has accumulated to cross a whole pixel, then jumps by one in a
// single frame. That reads as a robotic step-then-freeze on a slow layer regardless of size: a
// bigger star just makes the same jump a smaller fraction of its own footprint, not an actually
// smooth one. Weighting each touched pixel by its exact overlap fraction is the same fix
// hardware MSAA would give for free; this does it by hand rather than turning on multisampling
// for the whole window. Reduces to a single bilinear-weighted 2x2 splat when size is 1.
void DrawSoftSquare(float x, float y, int size, Color color) {
    const float sizeF = static_cast<float>(size);
    const int firstX = static_cast<int>(std::floor(x));
    const int firstY = static_cast<int>(std::floor(y));
    const int lastX = static_cast<int>(std::floor(x + sizeF));
    const int lastY = static_cast<int>(std::floor(y + sizeF));

    for (int py = firstY; py <= lastY; ++py) {
        const float coverageY = std::min(static_cast<float>(py) + 1.0f, y + sizeF) -
                                std::max(static_cast<float>(py), y);
        if (coverageY <= 0.0f) {
            continue;
        }
        for (int px = firstX; px <= lastX; ++px) {
            const float coverageX = std::min(static_cast<float>(px) + 1.0f, x + sizeF) -
                                    std::max(static_cast<float>(px), x);
            if (coverageX <= 0.0f) {
                continue;
            }
            Color blended = color;
            blended.a =
                static_cast<unsigned char>(static_cast<float>(color.a) * coverageX * coverageY);
            DrawPixel(px, py, blended);
        }
    }
}

void DrawStarfield(const std::vector<StarLayer>& layers, sr::Vec2 cameraTarget) {
    const float screenCenterX = static_cast<float>(GetScreenWidth()) * 0.5f;
    const float screenCenterY = static_cast<float>(GetScreenHeight()) * 0.5f;
    for (const StarLayer& layer : layers) {
        const float offsetX = cameraTarget.x * layer.parallax;
        const float offsetY = cameraTarget.y * layer.parallax;
        for (const Star& star : layer.stars) {
            const float x = WrapToTile(star.tilePos.x - offsetX, kStarTileUnits) + screenCenterX;
            const float y = WrapToTile(star.tilePos.y - offsetY, kStarTileUnits) + screenCenterY;
            DrawSoftSquare(x, y, layer.pixelSize, layer.color);
        }
    }
}

// Replaces the OS pointer (hidden once in main(), below) with a small crosshair reticle at the
// aim point FlightControls' AimIntent already reads off GetMousePosition() -- the pointer arrow
// reads as "click a menu," not "aim a weapon."
void DrawReticle(Vector2 pos) {
    constexpr float kRingRadius = 9.0f;
    constexpr float kDotRadius = 1.5f;
    constexpr float kTickGap = 3.0f;
    constexpr float kTickLength = 5.0f;
    constexpr Color kReticleColor{140, 255, 210, 235};

    DrawCircleLines(static_cast<int>(pos.x), static_cast<int>(pos.y), kRingRadius, kReticleColor);
    DrawCircleV(pos, kDotRadius, kReticleColor);

    const float innerR = kRingRadius + kTickGap;
    const float outerR = innerR + kTickLength;
    DrawLineEx(Vector2{pos.x, pos.y - innerR}, Vector2{pos.x, pos.y - outerR}, 1.5f, kReticleColor);
    DrawLineEx(Vector2{pos.x, pos.y + innerR}, Vector2{pos.x, pos.y + outerR}, 1.5f, kReticleColor);
    DrawLineEx(Vector2{pos.x - innerR, pos.y}, Vector2{pos.x - outerR, pos.y}, 1.5f, kReticleColor);
    DrawLineEx(Vector2{pos.x + innerR, pos.y}, Vector2{pos.x + outerR, pos.y}, 1.5f, kReticleColor);
}

// Fixed screen radius a ship shrinks to before it stops being drawn at true scale -- see
// kZoomMin's own comment in main(). architecture.md's WorldBody icon substitution
// (IconRenderer::NeedsIconSubstitution) uses 4px as its "a few pixels" floor for a passively
// shrinking planet/asteroid; a ship gets a deliberately larger, more legible floor since it is
// the one thing the player is always looking at. There is no production equivalent for a Rig
// yet -- WorldRenderer's icon substitution scopes entirely to `BodyKind` objects -- so this is
// sandbox-only until (if) it earns a real one.
constexpr float kShipIconRadiusPixels = 8.0f;

// A small fixed-pixel triangle pointing along `rotationRadians`, substituted for the ship's
// true-scale hull once zoom has shrunk it to kShipIconRadiusPixels -- the same "icon instead of
// an unreadable shrinking shape" idea IconRenderer::DrawBodyIcon applies to world bodies,
// extended here to the player's own hull. Screen-space, drawn outside DrawWorld's
// BeginMode2D/EndMode2D, the same layering rule DrawBodyIcon/DrawAimReticle already follow.
void DrawShipIcon(Vector2 screenPos, float rotationRadians, Color color) {
    const float cosA = std::cos(rotationRadians);
    const float sinA = std::sin(rotationRadians);
    const auto rotate = [&](float lx, float ly) {
        return Vector2{screenPos.x + (lx * cosA) - (ly * sinA),
                       screenPos.y + (lx * sinA) + (ly * cosA)};
    };
    const Vector2 nose = rotate(kShipIconRadiusPixels, 0.0f);
    const Vector2 tailLeft = rotate(-kShipIconRadiusPixels * 0.7f, kShipIconRadiusPixels * 0.6f);
    const Vector2 tailRight = rotate(-kShipIconRadiusPixels * 0.7f, -kShipIconRadiusPixels * 0.6f);
    DrawTriangle(nose, tailLeft, tailRight, color);
}

// The real production order (modes/space/systems/SystemSchedule.cpp's TickSchedule) restricted to
// the systems movement and combat need. PlayerLocationSystem runs first, deriving PlayerControlled
// onto the player's rig root every tick (architecture.md 12.30.1) -- TargetingSystem's own
// exclude<PlayerControlled> and NpcAiSystem's exclude<PlayerLocation> both depend on it, and
// without it (Milestone 6 and earlier never ran this system) the target dummy's AI would try to
// drive the player's own rig too. PowerSystem recomputes PowerBudget before PlayerInputSystem/
// WeaponSystem read it; PlayerInputSystem writes ThrustInput/FireIntent/AimPoint from this tick's
// intents; PhysicsSystem integrates thrust; HierarchySystem settles hardpoint transforms so
// TargetingSystem/WeaponSystem/ProjectileSystem all read this tick's positions, not last tick's
// stale ones (architecture.md 13.3 finding G, the same reason SystemSchedule.cpp runs
// HierarchySystem after PhysicsSystem); TargetingSystem acquires the target dummy's hostile lock
// on the player (Milestone 7: previously a no-op with nothing hostile in the world); NpcAiSystem
// turns that acquired Target into ThrustInput/FireIntent for the dummy, the same "reads Target,
// writes FireIntent" contract that puts it after TargetingSystem and before WeaponSystem in
// production (NpcAiSystem.h's own scheduling comment); WeaponSystem spawns projectiles (now from
// both sides); ProjectileSystem advances and hit-tests them; DamageSystem drains the PendingDamage
// that lands (now able to land on the player, not just the dummy). Add a call here, in schedule
// order, the day another system is the thing under test.
void TickSystems(const sr::space::SystemContext& ctx) {
    sr::space::player_location_system::Tick(ctx);
    sr::space::power_system::Tick(ctx);
    sr::space::player_input_system::Tick(ctx);
    sr::space::physics_system::Tick(ctx);
    sr::space::hierarchy_system::Tick(ctx);
    sr::space::targeting_system::Tick(ctx);
    sr::space::npc_ai_system::Tick(ctx);
    sr::space::weapon_system::Tick(ctx);
    sr::space::projectile_system::Tick(ctx);
    sr::space::damage_system::Tick(ctx);
}

}  // namespace

int main() {
    const std::filesystem::path contentDir = FindContentDirectory();
    if (contentDir.empty()) {
        std::cerr << "sandbox: could not locate data/base_game\n";
        return 1;
    }

    std::optional<sr::core::ContentLibrary> content = LoadContent(contentDir);
    if (!content.has_value()) {
        return 1;
    }
    content->RegisterDraftedTemplate(MakeWeaponVarietyBlueprint());

    sr::engine::Window window;
    if (!window.Open(1280, 720, "StarReach Sandbox")) {
        std::cerr << "sandbox: failed to open a window\n";
        return 1;
    }
    HideCursor();

    sr::space::SystemWorld world("sandbox");
    sr::core::IntentQueue intents;
    sr::core::FixedTimestep clock;
    // Milestone 7: TargetingSystem::IsHostile fails closed on a null diplomacy pointer
    // (TargetingSystem.cpp) -- without a real, seeded matrix here, the target dummy's "reapers"
    // faction override (SpawnTargetDummy's own comment) would never actually read as hostile.
    // Same seed order production uses at startup (RelationSeeding.h): baseline first, then the
    // Reapers' own escalation.
    sr::core::diplomacy::DiplomacyMatrix diplomacy;
    sr::core::diplomacy::SeedBaselineRelations(diplomacy);
    sr::core::diplomacy::SeedReaperHostility(diplomacy);
    const entt::entity player = SpawnPlayerShip(world, *content);
    const entt::entity targetDummy = SpawnTargetDummy(world, *content);
    const std::vector<ShieldDummy> shieldDummies = SpawnShieldDummies(world, *content);
    const std::vector<StarLayer> starLayers = MakeStarfield();
    const std::vector<NebulaLayer> nebulaLayers = MakeNebulae();

    // RigFactory already computed this rig's true bounding radius (CollisionRadius, world
    // units) -- deriving the zoom range from it rather than a hardcoded number means it keeps
    // matching whatever kSandboxBlueprint is, if that ever changes.
    const float shipRadius =
        player != entt::null ? world.Registry().get<sr::CollisionRadius>(player).value : 1.0f;
    // Max zoom: the ship's on-screen DIAMETER equals the middle third of the screen's height.
    constexpr float kShipMaxZoomScreenFraction = 1.0f / 3.0f;
    const float zoomMax =
        (static_cast<float>(window.Height()) * kShipMaxZoomScreenFraction) / (2.0f * shipRadius);
    // Min zoom, for now: the zoom at which the ship's true-scale radius has shrunk to exactly
    // kShipIconRadiusPixels -- past this point there is nothing smaller to show yet, so the
    // range simply stops here instead of continuing to shrink an unreadable hull.
    const float zoomMin = kShipIconRadiusPixels / shipRadius;
    float zoom = zoomMax;  // Start zoomed all the way in.

    sr::Vec2 cameraTarget{0.0f, 0.0f};

    while (!window.ShouldClose()) {
        // Multiplicative, not additive: a fixed pixel step would feel wrong at both ends of a
        // range this wide (screen-filling down to an 8px icon), the same reason map/photo-viewer
        // zoom always scales rather than adds. 1.35, not 1.12: at aegis_vanguard's real
        // CollisionRadius the zoomMax/zoomMin ratio is roughly 20-25x depending on monitor
        // height, and 1.12 needed ~28 notches to cross that -- easy to scroll partway, conclude
        // the icon does not exist, and stop. 1.35 crosses the same range in ~10.
        constexpr float kZoomStepPerNotch = 1.35f;
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            zoom = std::clamp(zoom * std::pow(kZoomStepPerNotch, wheel), zoomMin, zoomMax);
        }

        sr::space::ui::flight_controls::Poll(intents, kSandboxActorId,
                                             sr::space::render::CameraView{cameraTarget, zoom});

        clock.Advance(window.FrameTime());
        while (clock.ConsumeStep()) {
            const sr::space::SystemContext ctx{world,
                                               intents,
                                               *content,
                                               sr::core::kFixedDeltaSeconds,
                                               clock.ElapsedTicks(),
                                               nullptr,
                                               nullptr,
                                               &diplomacy,
                                               nullptr,
                                               nullptr};
            TickSystems(ctx);
        }
        intents.Clear();

        if (player != entt::null) {
            cameraTarget = world.Registry().get<sr::WorldTransform>(player).position;
        }

        window.BeginFrame();
        DrawNebulae(nebulaLayers, cameraTarget);
        DrawStarfield(starLayers, cameraTarget);
        // zoom cannot go lower than zoomMin, so this is also "we are all the way zoomed out."
        constexpr float kZoomEpsilon = 1e-4f;
        const bool useShipIcon = player != entt::null && zoom <= zoomMin + kZoomEpsilon;
        if (!useShipIcon) {
            sr::space::render::DrawWorld(world, sr::space::render::CameraView{cameraTarget, zoom},
                                         clock.Alpha());
        } else {
            const Vector2 screenCenter{static_cast<float>(window.Width()) * 0.5f,
                                       static_cast<float>(window.Height()) * 0.5f};
            const float heading = world.Registry().get<sr::WorldTransform>(player).rotation;
            DrawShipIcon(screenCenter, heading, RAYWHITE);
        }
        DrawText(
            "StarReach Sandbox -- W/A/S/D/Q/E move, mouse aims, LMB fires (hold on a target then "
            "keep holding for turret_lockon), 1-9 toggle weapon groups, scroll to zoom, close to "
            "quit",
            12, 12, 18, RAYWHITE);
        // Debug readout, sandbox-only: makes the zoom range's two ends directly verifiable
        // instead of having to guess whether a given amount of scrolling actually reached them.
        DrawText(TextFormat("zoom: %.3f  [min %.3f, max %.3f]  %s", zoom, zoomMin, zoomMax,
                            useShipIcon ? "(icon)" : "(hull)"),
                 12, 34, 18, RAYWHITE);
        // Debug readout, sandbox-only: the dummy's own hull carries no health bar in WorldRenderer
        // (Health.h: "no rig-wide health bar anywhere in this project"), so this is the only
        // on-screen confirmation that a landed shot actually drained something.
        const bool targetDestroyed =
            targetDummy == entt::null || world.Registry().all_of<sr::Destroyed>(targetDummy);
        DrawText(TextFormat("target dummy: %s", targetDestroyed ? "DESTROYED" : "alive"), 12, 56,
                 18, targetDestroyed ? RED : RAYWHITE);
        // Debug readout, sandbox-only: EnabledWeaponGroups has no production HUD anywhere in the
        // codebase (features.md 3.6's toggle exists, but nothing displays it), so this is the
        // only on-screen confirmation that keys 1-9 actually flipped the mask WeaponSystem reads.
        // Group 0 covers both wing_port and wing_aux (same ModuleId, features.md 3.6's per-
        // ModuleId grouping), so key 1 toggles that pair together.
        if (player != entt::null) {
            const auto& groups = world.Registry().get<sr::EnabledWeaponGroups>(player);
            DrawText(TextFormat("weapons 1-4 -- pulse(+aux): %s  autocannon: %s  flak: %s  "
                                "railgun: %s",
                                (groups.mask & 0x01u) ? "ON" : "off",
                                (groups.mask & 0x02u) ? "ON" : "off",
                                (groups.mask & 0x04u) ? "ON" : "off",
                                (groups.mask & 0x08u) ? "ON" : "off"),
                     12, 78, 18, RAYWHITE);
            DrawText(TextFormat(
                         "weapons 5-9 -- beam: %s  trackbeam: %s  seeker: %s  lockon: %s  "
                         "burst: %s",
                         (groups.mask & 0x10u) ? "ON" : "off", (groups.mask & 0x20u) ? "ON" : "off",
                         (groups.mask & 0x40u) ? "ON" : "off", (groups.mask & 0x80u) ? "ON" : "off",
                         (groups.mask & 0x100u) ? "ON" : "off"),
                     12, 100, 18, RAYWHITE);

            // Debug readout, sandbox-only: turret_lockon's Weapon carries the only genuinely
            // stateful new mechanic (a lock that must survive across ticks, a charge that must
            // reach chargeSecondsToFire before auto-firing, ammo that depletes and never
            // refills) -- none of it is visible anywhere else, so this is the only way to confirm
            // "pressed on nothing, no charge started" vs. "charging" vs. "locked and ready" vs.
            // "out of ammo" actually happened.
            const entt::entity lockon = sr::space::rig_factory::FindHardpoint(
                world.Registry(), player, sr::MountId("turret_lockon"));
            if (const auto* weapon = world.Registry().try_get<sr::Weapon>(lockon)) {
                DrawText(TextFormat("lockon -- ammo: %.0f/%.0f  charge: %.2f/%.2fs  %s",
                                    weapon->ammoRemaining, weapon->maxAmmo, weapon->chargeSeconds,
                                    weapon->chargeSecondsToFire,
                                    weapon->lockedTarget != entt::null ? "LOCKED" : ""),
                         12, 122, 18, weapon->lockedTarget != entt::null ? GREEN : RAYWHITE);
            }
        }
        // Milestone 6: one line per shield dummy, hull + shield state read live off the real
        // components DamageSystem mutates (see DrawShieldDummyHud's own comment).
        DrawShieldDummyHud(world.Registry(), shieldDummies, 150);
        DrawReticle(GetMousePosition());
        window.EndFrame();
    }

    ShowCursor();
    window.Close();
    return 0;
}
