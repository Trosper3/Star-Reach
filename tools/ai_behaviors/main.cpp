#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <random>
#include <string>
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
#include "modes/space/systems/MiningSystem.h"
#include "modes/space/systems/NpcAiSystem.h"
#include "modes/space/systems/OrbitSystem.h"
#include "modes/space/systems/PhysicsSystem.h"
#include "modes/space/systems/PlayerInputSystem.h"
#include "modes/space/systems/PlayerLocationSystem.h"
#include "modes/space/systems/ProjectileSystem.h"
#include "modes/space/systems/System.h"
#include "modes/space/systems/TargetingSystem.h"
#include "modes/space/systems/WeaponSystem.h"
#include "modes/space/ui/FlightControls.h"
#include "shared/blueprints/Ids.h"
#include "shared/blueprints/Validation.h"
#include "shared/components/Ai.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Mining.h"
#include "shared/components/Orbit.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"
#include "shared/math/Vec2.h"

// tools/ai_behaviors -- a sibling to tools/sandbox, deliberately separate rather than another
// milestone layered onto it: that tool showcases weapons/shields against passive dummies, this
// one showcases NpcAiSystem's real Patrol/Chase/Attack/Flee/Escort/Harvest state machine
// (modes/space/systems/NpcAiSystem.cpp) against a minimal world -- one player ship, a handful of
// NPC rigs seeded into distinct starting states, and an asteroid belt for spatial context and
// (for the Harvester) something real to drain. No weapon-variety roster, no shield-dummy matrix.
//
// Milestone 1: the roster below, asteroids purely decorative. Ship-asteroid collision/avoidance
// and ship-ship ramming (CollisionSystem) are deliberately out of TickSystems -- the same scoping
// discipline tools/sandbox/main.cpp's own milestones already follow (build what's under
// discussion, flag the rest rather than build it ad hoc).
//
// Milestone 2: Harvest. AiState::Harvest/AiBehavior::harvestTarget are a real production addition
// (NpcAiSystem.cpp), not sandbox-only -- Target (Targeting.h) can't be reused for a non-hostile
// resource target since TargetingSystem's own Tick stomps it every tick for any entity carrying
// one (Ai.h's own comment on harvestTarget). The Harvester below hand-sets harvestTarget to one
// of the belt's own asteroids, the same "no producer exists yet, hand-seed it here" idiom already
// used for the Escort's escortTarget. Draining flows through the real DamageSystem/MiningSystem
// pipeline unchanged, so the asteroids now need the Asteroid tag + AsteroidComposition
// MiningSystem's view requires -- not just decoration anymore. Deliberately NOT built: NPC
// pickup/cargo. LootSystem's own pickup is confirmed player-only (keyed on PlayerLocation), so
// the ElementDrop a depleted asteroid spawns just sits there unclaimed -- a real, flagged gap,
// the same shape as Flee's docking request with nowhere real to go in this sandbox.

namespace {

constexpr sr::ActorId kActorId{1};
// A real, validated, thruster-equipped fighter (data/base_game/ships.json) -- used for every rig
// here, player and NPC alike, so no custom blueprint needs drafting the way tools/sandbox's own
// MakeWeaponVarietyBlueprint does. It mounts no Sensor module, so SensorRange is hand-set below
// on every NPC that needs to detect a target.
constexpr const char* kShipBlueprint = "aegis_vanguard";

// Same walk-up src/main.cpp's own FindContentDirectory does -- duplicated rather than shared, the
// same small duplication tools/sandbox/main.cpp already carries for the same reason.
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
        std::cerr << "ai_behaviors: " << load.Summary() << "\n";
        return std::nullopt;
    }
    const sr::core::LoadReport validation = content.ValidateAll();
    if (!validation.ok()) {
        std::cerr << "ai_behaviors: " << validation.Summary() << "\n";
        return std::nullopt;
    }
    return content;
}

// rig_factory::Spawn only reports pass/fail (SpawnResult::ok()), not why -- this mirrors
// tools/sandbox/main.cpp's own ReportSpawnFailure so a bad blueprint/content edit fails loud
// instead of silently spawning nothing.
void ReportSpawnFailure(const sr::core::ContentLibrary& content,
                        const sr::BlueprintId& blueprintId) {
    const sr::ShipBlueprint* blueprint = content.FindShip(blueprintId);
    if (blueprint == nullptr) {
        std::cerr << "ai_behaviors: blueprint '" << blueprintId.str() << "' not found\n";
        return;
    }
    const sr::ValidationResult result = sr::Validate(*blueprint, content);
    std::cerr << "ai_behaviors: blueprint '" << blueprintId.str() << "' failed validation ("
              << result.errors.size() << " error(s)):\n";
    for (const sr::ValidationError& error : result.errors) {
        std::cerr << "  [" << sr::ToString(error.rule) << "] " << error.message << "\n";
    }
}

// Spawns the one rig the player flies, tagged with the same ActorRef FlightControls' intents and
// PlayerInputSystem's resolution already agree on. PlayerLocation is required so
// PlayerLocationSystem derives PlayerControlled onto it -- without that, NpcAiSystem's own
// exclude<PlayerLocation> (NpcAiSystem.cpp) would try to fly this rig too.
entt::entity SpawnPlayerShip(sr::space::SystemWorld& world,
                             const sr::core::ContentLibrary& content) {
    sr::space::rig_factory::SpawnParams params;
    params.blueprint = sr::BlueprintId(kShipBlueprint);
    params.position = sr::Vec2{0.0f, 0.0f};
    params.rotation = 0.0f;

    const sr::space::rig_factory::SpawnResult result =
        sr::space::rig_factory::Spawn(world, content, params);
    if (!result.ok()) {
        ReportSpawnFailure(content, params.blueprint);
        return entt::null;
    }
    world.Registry().emplace<sr::ActorRef>(result.root, sr::ActorRef{kActorId});
    world.Registry().emplace<sr::PlayerLocation>(result.root, sr::PlayerLocation{result.root});
    return result.root;
}

// Every NPC rig in the roster below comes through here: no ActorRef (PlayerInputSystem never
// touches it -- NpcAiSystem drives it instead), `faction` overrides the blueprint's authored
// aegis_directorate when non-empty (RigFactory::SpawnParams' own comment) -- leaving it default
// keeps the rig friendly to the player (DiplomacyMatrix::Get hard-codes same-faction to
// Relation::Friendly), "reapers" makes it hostile once SeedReaperHostility below runs.
entt::entity SpawnAiShip(sr::space::SystemWorld& world, const sr::core::ContentLibrary& content,
                         sr::Vec2 position, sr::FactionId faction) {
    sr::space::rig_factory::SpawnParams params;
    params.blueprint = sr::BlueprintId(kShipBlueprint);
    params.faction = std::move(faction);
    params.position = position;
    params.rotation = 0.0f;

    const sr::space::rig_factory::SpawnResult result =
        sr::space::rig_factory::Spawn(world, content, params);
    if (!result.ok()) {
        ReportSpawnFailure(content, params.blueprint);
        return entt::null;
    }
    world.Registry().get<sr::SensorRange>(result.root).units = 2000.0f;
    return result.root;
}

// Drops this rig's hardpoints to a fraction of their health so
// rig_attachment::AggregateStructuralIntegrity (shared/rig/ModuleAttachment.h) starts under
// NpcAiSystem's kFleeIntegrityFraction (0.5, NpcAiSystem.cpp) from tick one, rather than waiting
// for the player to shoot it down first to see the Flee state at all.
void PreDamageBelowFleeThreshold(entt::registry& registry, entt::entity rigRoot) {
    constexpr float kFleeDemoHealthFraction = 0.3f;
    const auto* rig = registry.try_get<sr::Rig>(rigRoot);
    if (rig == nullptr) {
        return;
    }
    for (const entt::entity child : rig->children) {
        if (auto* health = registry.try_get<sr::Health>(child)) {
            health->current = health->max * kFleeDemoHealthFraction;
        }
    }
}

using Rng = std::minstd_rand;

// A small, local reimplementation of WorldGen.cpp's own SpawnAsteroids -- that function lives in
// WorldGen.cpp's anonymous namespace, not exported via WorldGen.h, so it cannot be called from
// here without widening a production factory's public surface for a sandbox feature. Carries the
// Asteroid tag + a simple fixed AsteroidComposition (not WorldGen's randomized roll table -- a
// showcase doesn't need it) so MiningSystem's own view (Asteroid, AsteroidComposition,
// WorldTransform, Destroyed) actually consumes one once the Harvester drains it. Returns the
// spawned entities so the caller can hand one to the Harvester as a harvestTarget.
std::vector<entt::entity> SpawnAsteroids(entt::registry& registry, Rng& rng, int count) {
    constexpr float kBandMin = 1600.0f;
    constexpr float kBandMax = 2200.0f;
    constexpr float kRadius = 40.0f;
    constexpr float kHealth = 60.0f;
    std::uniform_real_distribution<float> distBand(kBandMin, kBandMax);
    std::uniform_real_distribution<float> distSpeed(0.01f, 0.03f);
    std::uniform_int_distribution<int> distSign(0, 1);

    std::vector<entt::entity> asteroids;
    for (int i = 0; i < count; ++i) {
        const float angle = (sr::kTwoPi * static_cast<float>(i)) / static_cast<float>(count);
        const float dist = distBand(rng);
        const sr::Vec2 position = sr::FromAngle(angle) * dist;
        const float speedMagnitude = distSpeed(rng);
        const float angularSpeed = distSign(rng) == 0 ? speedMagnitude : -speedMagnitude;

        sr::OrbitBody orbit{};
        orbit.center = entt::null;  // OrbitSystem treats a null center as orbiting the origin.
        orbit.radius = dist;
        orbit.angularSpeed = angularSpeed;
        orbit.phase = angle;

        const entt::entity asteroid = registry.create();
        registry.emplace<sr::Asteroid>(asteroid);
        registry.emplace<sr::Health>(asteroid, kHealth, kHealth);
        registry.emplace<sr::WorldTransform>(asteroid, position, 0.0f);
        registry.emplace<sr::PreviousTransform>(asteroid, position, 0.0f);
        registry.emplace<sr::OrbitBody>(asteroid, orbit);
        registry.emplace<sr::HitRadius>(asteroid, kRadius);
        registry.emplace<sr::WorldBody>(asteroid, kRadius, sr::BodyKind::Asteroid);
        registry.emplace<sr::AsteroidComposition>(
            asteroid, sr::AsteroidComposition{{sr::ElementChance{"iron", 100}}});
        asteroids.push_back(asteroid);
    }
    return asteroids;
}

struct AiRoster {
    entt::entity player = entt::null;
    entt::entity leader = entt::null;
    entt::entity escort = entt::null;
    entt::entity attacker = entt::null;
    entt::entity fleeing = entt::null;
    entt::entity harvester = entt::null;
    // The whole belt, kept around so main()'s loop can re-target the Harvester to the next one
    // once its current target is depleted -- see RetargetHarvester below.
    std::vector<entt::entity> asteroids;
};

// Builds the whole scene: diplomacy first (TargetingSystem::IsHostile fails closed on a null
// matrix), then the asteroid belt (the Harvester below needs one to target), then the roster --
// leader/escort/harvester share the player's own aegis_directorate faction (friendly),
// attacker/fleeing are "reapers" (at War with aegis_directorate once SeedReaperHostility runs).
AiRoster SetupWorld(sr::space::SystemWorld& world, const sr::core::ContentLibrary& content,
                    sr::core::diplomacy::DiplomacyMatrix& diplomacy) {
    sr::core::diplomacy::SeedBaselineRelations(diplomacy);
    sr::core::diplomacy::SeedReaperHostility(diplomacy);

    Rng rng(1234);
    const std::vector<entt::entity> asteroids = SpawnAsteroids(world.Registry(), rng, 6);

    AiRoster roster;
    roster.player = SpawnPlayerShip(world, content);
    roster.leader = SpawnAiShip(world, content, sr::Vec2{600.0f, 0.0f}, sr::FactionId());
    roster.escort = SpawnAiShip(world, content, sr::Vec2{600.0f, 150.0f}, sr::FactionId());
    roster.attacker =
        SpawnAiShip(world, content, sr::Vec2{-900.0f, 400.0f}, sr::FactionId("reapers"));
    roster.fleeing =
        SpawnAiShip(world, content, sr::Vec2{-900.0f, -400.0f}, sr::FactionId("reapers"));
    roster.harvester = SpawnAiShip(world, content, sr::Vec2{1700.0f, 200.0f}, sr::FactionId());

    // Escort: no order producer writes AiBehavior::escortTarget anywhere in the shipped game yet
    // (Ai.h's own comment), so it has to be hand-set here, before the first NpcAiSystem tick, for
    // NpcAiSystem's own PatrolOrEscort to resolve AiState::Escort at all.
    if (roster.escort != entt::null && roster.leader != entt::null) {
        world.Registry().emplace<sr::AiBehavior>(
            roster.escort, sr::AiBehavior{sr::AiState::Patrol, roster.leader});
    }
    // Harvest: same idiom as Escort above -- harvestTarget has no producer in the shipped game
    // yet either, so it's hand-set to one of the belt's own asteroids.
    if (roster.harvester != entt::null && !asteroids.empty()) {
        world.Registry().emplace<sr::AiBehavior>(
            roster.harvester, sr::AiBehavior{sr::AiState::Patrol, entt::null, asteroids.front()});
    }
    if (roster.fleeing != entt::null) {
        PreDamageBelowFleeThreshold(world.Registry(), roster.fleeing);
    }

    roster.asteroids = std::move(asteroids);
    return roster;
}

// A one-shot harvest run empties its asteroid in a handful of seconds -- with nothing to
// reassign harvestTarget afterward, the Harvester would sit in Patrol for the rest of the
// session, making the state easy to miss entirely. NpcAiSystem itself deliberately has no
// producer for this (Ai.h's own comment on harvestTarget -- nothing in the shipped game
// reassigns it either), so this stays sandbox-only orchestration: once TryHarvest clears a
// depleted target, hand the Harvester the next live asteroid in the belt.
void RetargetHarvester(entt::registry& registry, const AiRoster& roster) {
    if (roster.harvester == entt::null) {
        return;
    }
    auto* behavior = registry.try_get<sr::AiBehavior>(roster.harvester);
    if (behavior == nullptr || behavior->harvestTarget != entt::null) {
        return;
    }
    for (const entt::entity asteroid : roster.asteroids) {
        if (registry.valid(asteroid)) {
            behavior->harvestTarget = asteroid;
            return;
        }
    }
}

const char* AiStateName(sr::AiState state) {
    switch (state) {
        case sr::AiState::Patrol: return "Patrol";
        case sr::AiState::Chase: return "Chase";
        case sr::AiState::Attack: return "Attack";
        case sr::AiState::Flee: return "Flee";
        case sr::AiState::Escort: return "Escort";
        case sr::AiState::Harvest: return "Harvest";
    }
    return "?";
}

struct AiHudEntry {
    const char* label;
    entt::entity entity;
};

// The whole point of this tool: makes each NPC's live AiState visible, the same "no in-game
// health bar anywhere in this project" reasoning behind tools/sandbox's own DrawShieldDummyHud
// (Milestone 6). AiBehavior is get_or_emplace'd lazily by NpcAiSystem's first tick on it, so it
// may not exist for a frame or two after spawn -- read defensively rather than asserting.
void DrawAiStateHud(const entt::registry& registry, const std::vector<AiHudEntry>& entries,
                    int startY) {
    int y = startY;
    for (const AiHudEntry& entry : entries) {
        if (entry.entity == entt::null) {
            DrawText(TextFormat("%s: (failed to spawn)", entry.label), 12, y, 16, RED);
            y += 20;
            continue;
        }
        const bool destroyed = registry.all_of<sr::Destroyed>(entry.entity);
        const auto* behavior = registry.try_get<sr::AiBehavior>(entry.entity);
        const char* state =
            destroyed ? "DESTROYED"
                      : (behavior != nullptr ? AiStateName(behavior->state) : "(pending)");
        DrawText(TextFormat("%s: %s", entry.label, state), 12, y, 16, destroyed ? RED : RAYWHITE);
        y += 20;
    }
}

// Replaces the OS pointer (hidden once in main(), below) with a small crosshair reticle at the
// aim point FlightControls' AimIntent already reads off GetMousePosition() -- copied verbatim
// from tools/sandbox/main.cpp's own DrawReticle.
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

// The real production order (modes/space/systems/SystemSchedule.cpp's TickSchedule) restricted to
// what AI behavior needs, the same restriction tools/sandbox/main.cpp's own TickSystems applies.
// PowerSystem is omitted: WeaponSystem::RigSatisfaction falls back to full satisfaction with no
// PowerBudget present, and TargetingSystem never reads power at all. CollisionSystem/PartySystem
// are omitted too -- ramming and formation-offset/shared-retaliation are separate mechanisms from
// the AiState machine this tool showcases (NpcAiSystem's own PatrolOrEscort already does
// Escort's station-keeping unassisted, Ai.h's "deliberately not PartyMember" comment). OrbitSystem
// is new versus the weapons/shields sandbox's list -- needed so the asteroid belt's OrbitBody
// actually animates instead of sitting inert. MiningSystem is new for Milestone 2 -- runs after
// DamageSystem, matching production order (SystemSchedule.cpp) -- so a Harvester-drained asteroid
// actually rolls salvage and gets destroyed instead of sitting tagged Destroyed forever.
void TickSystems(const sr::space::SystemContext& ctx) {
    sr::space::player_location_system::Tick(ctx);
    sr::space::player_input_system::Tick(ctx);
    sr::space::physics_system::Tick(ctx);
    sr::space::hierarchy_system::Tick(ctx);
    sr::space::orbit_system::Tick(ctx);
    sr::space::targeting_system::Tick(ctx);
    sr::space::npc_ai_system::Tick(ctx);
    sr::space::weapon_system::Tick(ctx);
    sr::space::projectile_system::Tick(ctx);
    sr::space::damage_system::Tick(ctx);
    sr::space::mining_system::Tick(ctx);
}

}  // namespace

int main() {
    const std::filesystem::path contentDir = FindContentDirectory();
    if (contentDir.empty()) {
        std::cerr << "ai_behaviors: could not locate data/base_game\n";
        return 1;
    }

    std::optional<sr::core::ContentLibrary> content = LoadContent(contentDir);
    if (!content.has_value()) {
        return 1;
    }

    sr::engine::Window window;
    if (!window.Open(1280, 720, "StarReach AI Behaviors")) {
        std::cerr << "ai_behaviors: failed to open a window\n";
        return 1;
    }
    HideCursor();

    sr::space::SystemWorld world("ai_behaviors");
    sr::core::IntentQueue intents;
    sr::core::FixedTimestep clock;
    sr::core::diplomacy::DiplomacyMatrix diplomacy;
    const AiRoster roster = SetupWorld(world, *content, diplomacy);

    const std::vector<AiHudEntry> hudEntries = {
        {"Patrol leader", roster.leader},      {"Escort", roster.escort},
        {"Hostile attacker", roster.attacker}, {"Hostile (weakened)", roster.fleeing},
        {"Harvester", roster.harvester},
    };

    // Fixed bounds rather than tools/sandbox's own ship-radius-derived zoomMin/zoomMax: this
    // scene's own scale is set by the asteroid belt (out to ~2200 units), not by one hull.
    constexpr float kZoomMin = 0.05f;
    constexpr float kZoomMax = 1.0f;
    constexpr float kZoomStepPerNotch = 1.35f;
    float zoom = 0.2f;
    sr::Vec2 cameraTarget{0.0f, 0.0f};

    while (!window.ShouldClose()) {
        const float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            zoom = std::clamp(zoom * std::pow(kZoomStepPerNotch, wheel), kZoomMin, kZoomMax);
        }

        sr::space::ui::flight_controls::Poll(intents, kActorId,
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
            RetargetHarvester(world.Registry(), roster);
        }
        intents.Clear();

        if (roster.player != entt::null) {
            cameraTarget = world.Registry().get<sr::WorldTransform>(roster.player).position;
        }

        window.BeginFrame();
        sr::space::render::DrawWorld(world, sr::space::render::CameraView{cameraTarget, zoom},
                                     clock.Alpha());
        DrawText(
            "StarReach AI Behaviors -- W/A/S/D/Q/E move, mouse aims, LMB fires, scroll to zoom, "
            "close to quit",
            12, 12, 18, RAYWHITE);
        DrawAiStateHud(world.Registry(), hudEntries, 40);
        DrawReticle(GetMousePosition());
        window.EndFrame();
    }

    ShowCursor();
    window.Close();
    return 0;
}
