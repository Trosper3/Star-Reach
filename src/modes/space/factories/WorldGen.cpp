#include "modes/space/factories/WorldGen.h"

#include <cstddef>
#include <random>
#include <vector>

#include "modes/space/factories/NpcFactory.h"
#include "modes/space/factories/StationFactory.h"
#include "shared/blueprints/Ids.h"
#include "shared/components/Health.h"
#include "shared/components/Lighting.h"
#include "shared/components/Mining.h"
#include "shared/components/Orbit.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"
#include "shared/math/Vec2.h"

namespace sr::space::world_gen {
namespace {

using Rng = std::minstd_rand;

float RandomFloat(Rng& rng, float lo, float hi) {
    std::uniform_real_distribution<float> dist(lo, hi);
    return dist(rng);
}

int RandomInt(Rng& rng, int lo, int hi) {
    std::uniform_int_distribution<int> dist(lo, hi);
    return dist(rng);
}

// -- Sun ------------------------------------------------------------------------------------

constexpr float kSunGravityRange = 2200.0f;
constexpr float kSunGravityStrength = 260.0f;
constexpr float kSunLightRange = 6000.0f;
constexpr float kSunRadius = 350.0f;             // The visible disc.
constexpr float kCoronaRange = 1200.0f;          // Burning begins -- inside kSunGravityRange.
constexpr float kCoronaDamagePerSecond = 60.0f;  // At the centre.

void SpawnSun(entt::registry& registry) {
    const entt::entity sun = registry.create();
    registry.emplace<WorldTransform>(sun, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<GravityWell>(sun, kSunGravityRange, kSunGravityStrength);
    registry.emplace<LightSource>(sun, kSunLightRange);
    registry.emplace<WorldBody>(sun, kSunRadius, BodyKind::Star);
    registry.emplace<Corona>(sun, kCoronaRange, kCoronaDamagePerSecond);
}

// -- Planets ----------------------------------------------------------------------------------

constexpr float kMinOrbitRadius = 3200.0f;
constexpr float kOrbitSpacing = 1400.0f;
constexpr float kOrbitJitter = 400.0f;
constexpr float kMinAngularSpeed = 0.015f;
constexpr float kMaxAngularSpeed = 0.05f;
constexpr float kPlanetRadius = 80.0f;

// Planet type/zone variety (legacy PlanetTypeRegistry) has no JSON schema in this project yet
// (Law 10) -- every planet below is mechanically identical aside from its orbit. That is a
// content-pipeline follow-up, not something to hardcode as a second, unauthored table here.
void SpawnPlanets(entt::registry& registry, Rng& rng, int count) {
    for (int i = 0; i < count; ++i) {
        const float radius = kMinOrbitRadius + static_cast<float>(i) * kOrbitSpacing +
                             RandomFloat(rng, 0.0f, kOrbitJitter);
        const float speedMagnitude = RandomFloat(rng, kMinAngularSpeed, kMaxAngularSpeed);
        const float angularSpeed = RandomInt(rng, 0, 1) == 0 ? speedMagnitude : -speedMagnitude;
        const float phase = RandomFloat(rng, 0.0f, kTwoPi);

        OrbitBody orbit{};
        orbit.center = entt::null;
        orbit.radius = radius;
        orbit.angularSpeed = angularSpeed;
        orbit.phase = phase;

        const Vec2 position = FromAngle(phase) * radius;

        const entt::entity planet = registry.create();
        registry.emplace<OrbitBody>(planet, orbit);
        registry.emplace<WorldTransform>(planet, position, 0.0f);
        registry.emplace<PreviousTransform>(planet, position, 0.0f);
        registry.emplace<WorldBody>(planet, kPlanetRadius, BodyKind::Planet);
    }
}

// -- Asteroids --------------------------------------------------------------------------------

// Moved out from 1200-2200: the point of no return (where kSunGravityRange out-accelerates a
// fighter) is ~1500, so the old band's inner half sat inside the zone a fighter cannot climb out
// of under thrust -- harmless only while asteroids were unshootable. This puts the whole belt
// outside both the corona burn and the point of no return, with a clean gap before the innermost
// planet at kMinOrbitRadius.
constexpr float kAsteroidBandMin = 1800.0f;
constexpr float kAsteroidBandMax = 2800.0f;
constexpr float kAsteroidHealth = 60.0f;
constexpr float kAsteroidRadius = 40.0f;
constexpr float kAsteroidMinAngularSpeed = 0.01f;
constexpr float kAsteroidMaxAngularSpeed = 0.03f;

struct ElementPoolEntry {
    const char* id;
    int minPercent;
    int maxPercent;
};

// A trimmed, factory-internal tuning table -- the same category as MiningSystem's own hash-based
// salvage roll, not authored player-facing content (Law 10 governs ship/module/shell definitions
// in data/base_game/, not procedural-generation weighting like this).
constexpr ElementPoolEntry kElementPool[] = {
    {"iron", 40, 80},
    {"carbon", 35, 70},
    {"silica", 30, 65},
    {"titanium", 20, 45},
};
constexpr int kElementPoolSize = 4;

AsteroidComposition RollComposition(Rng& rng) {
    AsteroidComposition composition;
    const int elementCount = RandomInt(rng, 1, 2);
    for (int i = 0; i < elementCount; ++i) {
        const ElementPoolEntry& entry = kElementPool[RandomInt(rng, 0, kElementPoolSize - 1)];
        composition.elements.push_back(
            ElementChance{entry.id, RandomInt(rng, entry.minPercent, entry.maxPercent)});
    }
    return composition;
}

// Asteroids orbit like planets rather than drifting under Velocity -- OrbitSystem excludes
// orbiting bodies from GravityWell, so the belt circles the star indefinitely and
// deterministically instead of every asteroid inside kSunGravityRange accelerating unbounded into
// it (an asteroid has neither LinearDamping nor a maxSpeed clamp).
void SpawnAsteroids(entt::registry& registry, Rng& rng, int count) {
    for (int i = 0; i < count; ++i) {
        const float angle = (kTwoPi * static_cast<float>(i)) / static_cast<float>(count);
        const float dist = RandomFloat(rng, kAsteroidBandMin, kAsteroidBandMax);
        const Vec2 position = FromAngle(angle) * dist;
        const float speedMagnitude =
            RandomFloat(rng, kAsteroidMinAngularSpeed, kAsteroidMaxAngularSpeed);
        const float angularSpeed = RandomInt(rng, 0, 1) == 0 ? speedMagnitude : -speedMagnitude;
        const AsteroidComposition composition = RollComposition(rng);

        OrbitBody orbit{};
        orbit.center = entt::null;
        orbit.radius = dist;
        orbit.angularSpeed = angularSpeed;
        orbit.phase = angle;

        const entt::entity asteroid = registry.create();
        registry.emplace<Asteroid>(asteroid);
        registry.emplace<Health>(asteroid, kAsteroidHealth, kAsteroidHealth);
        registry.emplace<WorldTransform>(asteroid, position, 0.0f);
        registry.emplace<PreviousTransform>(asteroid, position, 0.0f);
        registry.emplace<OrbitBody>(asteroid, orbit);
        registry.emplace<HitRadius>(asteroid, kAsteroidRadius);
        registry.emplace<WorldBody>(asteroid, kAsteroidRadius, BodyKind::Asteroid);
        registry.emplace<AsteroidComposition>(asteroid, composition);
    }
}

// -- Station ----------------------------------------------------------------------------------

// The content set's only station blueprint (architecture.md 12.24 step 4) -- it already carries a
// docking_bay_i on a shell_facility_bay, and authors the same faction the player spawns into.
constexpr const char* kStationBlueprintId = "aegis_outpost";

// Outside kSunGravityRange (2200), or OrbitSystem's gravity well drags an engineless hull; close
// enough that step 2 (player control) is still judgeable, the same reachability kNpcBandMin/Max
// below give the NPC presence.
constexpr float kStationDistanceMin = 2400.0f;
constexpr float kStationDistanceMax = 3000.0f;

void SpawnStation(SystemWorld& world, const core::ContentLibrary& content, Rng& rng) {
    const float angle = RandomFloat(rng, 0.0f, kTwoPi);
    const float distance = RandomFloat(rng, kStationDistanceMin, kStationDistanceMax);

    station_factory::SpawnParams params;
    params.blueprint = BlueprintId(kStationBlueprintId);
    // Faction left default (empty): aegis_outpost authors aegis_directorate, the same faction the
    // player spawns into, which is what DockingSystem's FactionRef-equality gate requires -- a
    // neutral or hostile station would be undockable.
    params.position = FromAngle(angle) * distance;
    params.rotation = RandomFloat(rng, 0.0f, kTwoPi);

    station_factory::Spawn(world, content, params);
}

// -- NPC presence -----------------------------------------------------------------------------

constexpr float kNpcBandMin = 1800.0f;
constexpr float kNpcBandMax = 3200.0f;
constexpr float kNpcMinSeparation = 250.0f;
constexpr int kNpcPlacementAttempts = 20;

bool TooCloseToAny(const std::vector<Vec2>& placed, const Vec2& candidate) {
    for (const Vec2& other : placed) {
        if (Distance(other, candidate) < kNpcMinSeparation) {
            return true;
        }
    }
    return false;
}

// Ported down from legacy WorldGen.cpp's retry-until-clear pattern: an annulus scatter with a
// bounded number of attempts, falling back to the last-rolled candidate rather than failing
// outright, since a handful of NPCs at initial world generation is never so dense that giving up
// is the right answer.
Vec2 PickNpcPosition(Rng& rng, const std::vector<Vec2>& placed) {
    Vec2 candidate{0.0f, 0.0f};
    for (int attempt = 0; attempt < kNpcPlacementAttempts; ++attempt) {
        const float angle = RandomFloat(rng, 0.0f, kTwoPi);
        const float dist = RandomFloat(rng, kNpcBandMin, kNpcBandMax);
        candidate = FromAngle(angle) * dist;
        if (!TooCloseToAny(placed, candidate)) {
            break;
        }
    }
    return candidate;
}

// Mobile blueprints only -- content.ShipIds() returns every authored ship, station blueprints
// (aegis_outpost) included, and nothing about "NPC presence" should ever be able to roll one.
// Found the hard way: on a std::hash<std::string> implementation that differs from the one this
// was first tested against, a different seed landed on exactly that roll, producing a second,
// fully dockable station wandering the system as an "NPC" -- std::hash's value is not portable
// across standard library implementations, so this was always going to happen on some platform.
std::vector<BlueprintId> MobileShipIds(const core::ContentLibrary& content) {
    std::vector<BlueprintId> ids;
    for (const BlueprintId& id : content.ShipIds()) {
        const ShipBlueprint* blueprint = content.FindShip(id);
        if (blueprint != nullptr && blueprint->mobile) {
            ids.push_back(id);
        }
    }
    return ids;
}

void SpawnNpcPresence(SystemWorld& world, const core::ContentLibrary& content, Rng& rng,
                      int count) {
    const std::vector<BlueprintId> shipIds = MobileShipIds(content);
    if (shipIds.empty()) {
        return;  // No mobile content loaded -- nothing to spawn from.
    }

    std::vector<Vec2> placed;
    placed.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const std::size_t pick =
            static_cast<std::size_t>(RandomInt(rng, 0, static_cast<int>(shipIds.size()) - 1));

        npc_factory::SpawnParams params;
        params.blueprint = shipIds[pick];
        params.position = PickNpcPosition(rng, placed);
        params.rotation = RandomFloat(rng, 0.0f, kTwoPi);

        const npc_factory::SpawnResult result = npc_factory::Spawn(world, content, params);
        if (result.ok()) {
            placed.push_back(params.position);
        }
    }
}

}  // namespace

void PopulateSystem(SystemWorld& world, const core::ContentLibrary& content, unsigned int seed) {
    Rng rng(seed);
    entt::registry& registry = world.Registry();

    SpawnSun(registry);
    SpawnPlanets(registry, rng, RandomInt(rng, 2, 6));
    SpawnAsteroids(registry, rng, 8);
    // Before SpawnNpcPresence, at a fixed point in the rng sequence, so adding the station does
    // not shift the NPC layout for a given seed.
    SpawnStation(world, content, rng);
    SpawnNpcPresence(world, content, rng, RandomInt(rng, 3, 5));
}

}  // namespace sr::space::world_gen
