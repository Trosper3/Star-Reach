#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/factories/WorldGen.h"
#include "modes/space/systems/OrbitSystem.h"
#include "modes/space/systems/SpawnSystem.h"
#include "shared/components/Docking.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Loot.h"
#include "shared/components/Mining.h"
#include "shared/components/Orbit.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Spawn.h"
#include "shared/components/Transform.h"
#include "shared/rig/CargoView.h"

using Catch::Approx;
using sr::Asteroid;
using sr::AsteroidComposition;
using sr::BodyKind;
using sr::DockingBay;
using sr::FactionId;
using sr::FactionRef;
using sr::GravityWell;
using sr::Health;
using sr::HitRadius;
using sr::Length;
using sr::OrbitBody;
using sr::PlayerControlled;
using sr::RespawnPending;
using sr::Rig;
using sr::SpawnAnchor;
using sr::Velocity;
using sr::WorldBody;
using sr::WorldTransform;
using sr::core::ContentLibrary;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace world_gen = sr::space::world_gen;
namespace orbit_system = sr::space::orbit_system;
namespace spawn_system = sr::space::spawn_system;
namespace cargo_view = sr::cargo_view;

namespace {

ContentLibrary Content() {
    ContentLibrary library;
    const auto report = library.LoadFromDirectory(std::filesystem::path(SR_DATA_DIR));
    REQUIRE(report.ok());
    return library;
}

}  // namespace

TEST_CASE("PopulateSystem seeds exactly one sun", "[world_gen]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    const auto view = world.Registry().view<GravityWell>();
    CHECK(std::distance(view.begin(), view.end()) == 1);
}

TEST_CASE("PopulateSystem seeds a plausible number of planets", "[world_gen]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    // Asteroids now carry OrbitBody too (architecture.md 12.28) -- exclude them so this stays a
    // planet count, not a "everything that orbits" count.
    const auto view = world.Registry().view<OrbitBody>(entt::exclude<Asteroid>);
    const auto count = std::distance(view.begin(), view.end());
    CHECK(count >= 2);
    CHECK(count <= 6);
}

TEST_CASE("PopulateSystem seeds exactly eight asteroids with rolled composition", "[world_gen]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    const auto view = registry.view<Asteroid, Health, AsteroidComposition>();
    CHECK(std::distance(view.begin(), view.end()) == 8);

    for (auto [entity, composition] : registry.view<AsteroidComposition>().each()) {
        (void)entity;
        CHECK_FALSE(composition.elements.empty());
        for (const auto& element : composition.elements) {
            CHECK(element.percent >= 1);
            CHECK(element.percent <= 100);
        }
    }
}

TEST_CASE("PopulateSystem seeds a plausible NPC presence with no PlayerControlled entity",
          "[world_gen]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    // architecture.md 12.24 step 4: the station added to PopulateSystem's output is a Rig too, so
    // the NPC-only band becomes one wider on each end -- one station plus the NPC roll.
    const auto view = registry.view<Rig>(entt::exclude<PlayerControlled>);
    const auto count = std::distance(view.begin(), view.end());
    CHECK(count >= 4);
    CHECK(count <= 6);
    CHECK(registry.view<PlayerControlled>().empty());
}

TEST_CASE("PopulateSystem seeds exactly one dockable station of the player's faction",
          "[world_gen]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    // SpawnAnchor, not DockingBay: the NPC-presence roll can coincidentally also pick
    // aegis_outpost's blueprint id off content.ShipIds() (it draws from every ShipBlueprint, not
    // just mobile ones), which would carry a DockingBay too but never goes through
    // station_factory::Spawn -- so it gets none of SpawnAnchor/StationFacility/Wallet/NetworkOwner.
    // SpawnAnchor is the tag only WorldGen's own station spawn ever emplaces.
    const auto anchorView = registry.view<SpawnAnchor>();
    REQUIRE(std::distance(anchorView.begin(), anchorView.end()) == 1);
    const entt::entity station = *anchorView.begin();

    bool hasDockingBay = false;
    for (const entt::entity hardpoint : registry.get<Rig>(station).children) {
        hasDockingBay |= registry.all_of<DockingBay>(hardpoint);
    }
    CHECK(hasDockingBay);

    const auto* playerBlueprint = content.FindShip(sr::BlueprintId("aegis_vanguard"));
    REQUIRE(playerBlueprint != nullptr);
    CHECK(registry.get<FactionRef>(station).id == playerBlueprint->faction);
}

TEST_CASE("PopulateSystem's station has a non-zero cargo hold that accepts and refuses deposits",
          "[world_gen]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    const auto anchorView = registry.view<SpawnAnchor>();
    REQUIRE(std::distance(anchorView.begin(), anchorView.end()) == 1);
    const entt::entity station = *anchorView.begin();

    CHECK(cargo_view::Capacity(registry, station) > 0.0f);

    const sr::ItemStack fits{sr::ItemKind::Element, "iron", 1, 4.0f};
    CHECK(cargo_view::Deposit(registry, station, fits) == cargo_view::DepositResult::Deposited);

    const sr::ItemStack doesNotFit{sr::ItemKind::Element, "iron", 1, 10000.0f};
    CHECK(cargo_view::Deposit(registry, station, doesNotFit) ==
          cargo_view::DepositResult::HoldFull);
}

TEST_CASE("PopulateSystem's station gives SpawnSystem an anchor to resolve against",
          "[world_gen]") {
    // architecture.md 13.3 R: SpawnSystem was wholly inert because SpawnAnchor had zero producers
    // -- FindNearestAnchor returned false and RespawnPending never resolved. This exercises the
    // real generated world end to end, not a hand-built anchor.
    //
    // architecture.md 12.36: the respawning rig's faction deliberately does NOT match the
    // station's (aegis_directorate), so ResolveSpawnPlacement's DockingBay-exit tier finds no
    // friendly bay and falls through to this test's actual subject, the anchor tier.
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    const auto anchorView = registry.view<SpawnAnchor>();
    REQUIRE(std::distance(anchorView.begin(), anchorView.end()) == 1);

    const entt::entity respawning = registry.create();
    registry.emplace<WorldTransform>(respawning, sr::Vec2{50000.0f, 50000.0f}, 0.0f);
    registry.emplace<RespawnPending>(respawning, 80.0f);
    registry.emplace<FactionRef>(respawning, FactionId("unaligned"));

    sr::core::IntentQueue intents;
    spawn_system::Tick(SystemContext{world, intents, content, 1.0f / 60.0f, 0});

    CHECK_FALSE(registry.all_of<RespawnPending>(respawning));
}

TEST_CASE("PopulateSystem's station anchor culls a rig 25,000 units out", "[world_gen]") {
    // architecture.md 13.3 R: CullFarRigs never ran because SpawnAnchor had zero producers.
    // Exercises the real generated world's own anchor, not a hand-built one -- the same station
    // this file's other SpawnAnchor tests already resolve against.
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    const auto anchorView = registry.view<SpawnAnchor>();
    REQUIRE(std::distance(anchorView.begin(), anchorView.end()) == 1);
    const sr::Vec2 anchorPosition = registry.get<WorldTransform>(*anchorView.begin()).position;

    const entt::entity far = registry.create();
    registry.emplace<WorldTransform>(far, anchorPosition + sr::Vec2{25000.0f, 0.0f}, 0.0f);
    Rig rig;
    const entt::entity hardpoint = registry.create();
    rig.children.push_back(hardpoint);
    registry.emplace<Rig>(far, rig);

    sr::core::IntentQueue intents;
    spawn_system::Tick(SystemContext{world, intents, content, 1.0f / 60.0f, 0});

    CHECK_FALSE(registry.valid(far));
    CHECK_FALSE(registry.valid(hardpoint));
}

TEST_CASE(
    "PopulateSystem emits a WorldBody of each of Star, Planet and Asteroid, and every "
    "entity with WorldTransform also carries WorldBody",
    "[world_gen]") {
    // The assertion that would have caught architecture.md 12.28 finding A: the sun, every
    // planet and every asteroid satisfied neither WorldRenderer view, so nothing drew them.
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    bool sawStar = false;
    bool sawPlanet = false;
    bool sawAsteroid = false;
    for (auto [entity, body] : registry.view<WorldBody>().each()) {
        (void)entity;
        sawStar |= body.kind == BodyKind::Star;
        sawPlanet |= body.kind == BodyKind::Planet;
        sawAsteroid |= body.kind == BodyKind::Asteroid;
    }
    CHECK(sawStar);
    CHECK(sawPlanet);
    CHECK(sawAsteroid);

    for (auto [entity, xf] : registry.view<WorldTransform>().each()) {
        (void)xf;
        // NPC rig roots and hardpoints carry WorldTransform too, but are excluded from WorldBody
        // by design (WorldRenderer.h): a rig is not one drawable. Restrict the blanket check to
        // non-rig bodies, which is exactly the set WorldGen itself spawns.
        if (!registry.all_of<Rig>(entity) && !registry.all_of<sr::ParentRig>(entity)) {
            CHECK(registry.all_of<WorldBody>(entity));
        }
    }
}

TEST_CASE("PopulateSystem's asteroids carry OrbitBody and HitRadius, not Velocity", "[world_gen]") {
    const ContentLibrary content = Content();
    SystemWorld world("sol");

    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    for (const entt::entity entity : registry.view<Asteroid>()) {
        CHECK(registry.all_of<OrbitBody>(entity));
        CHECK(registry.all_of<HitRadius>(entity));
        CHECK_FALSE(registry.all_of<Velocity>(entity));

        const auto& orbit = registry.get<OrbitBody>(entity);
        CHECK(orbit.radius >= 1800.0f);
        CHECK(orbit.radius <= 2800.0f);
    }
}

TEST_CASE("An asteroid at the belt's inner edge is still there after 10,000 ticks", "[world_gen]") {
    // Regression test for the OrbitBody-not-BodyMass decision (architecture.md 12.28): an
    // asteroid with Velocity and no damping inside the gravity well would accelerate unbounded
    // into the star and the belt would drain.
    const ContentLibrary content = Content();
    SystemWorld world("sol");
    world_gen::PopulateSystem(world, content, 42u);

    entt::registry& registry = world.Registry();
    entt::entity innermost = entt::null;
    float innermostRadius = 1.0e9f;
    for (auto [entity, orbit] : registry.view<OrbitBody, Asteroid>().each()) {
        if (orbit.radius < innermostRadius) {
            innermostRadius = orbit.radius;
            innermost = entity;
        }
    }
    REQUIRE((innermost != entt::null));

    sr::core::IntentQueue intents;
    for (int tick = 0; tick < 10000; ++tick) {
        orbit_system::Tick(SystemContext{world, intents, content, 1.0f / 60.0f,
                                         static_cast<unsigned long long>(tick)});
    }

    const float distance = Length(registry.get<WorldTransform>(innermost).position);
    CHECK(distance == Approx(innermostRadius).margin(0.5f));
}

TEST_CASE("PopulateSystem is deterministic for a given seed", "[world_gen]") {
    const ContentLibrary content = Content();

    SystemWorld worldA("sol");
    world_gen::PopulateSystem(worldA, content, 1234u);
    SystemWorld worldB("sol");
    world_gen::PopulateSystem(worldB, content, 1234u);

    const auto countOf = [](SystemWorld& world) {
        entt::registry& registry = world.Registry();
        return std::make_tuple(registry.view<OrbitBody>().size(), registry.view<Asteroid>().size(),
                               registry.view<Rig>().size());
    };
    CHECK(countOf(worldA) == countOf(worldB));
}
