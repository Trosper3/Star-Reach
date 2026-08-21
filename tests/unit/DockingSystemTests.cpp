#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/diplomacy/DiplomacyMatrix.h"
#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/DockingSystem.h"
#include "shared/components/Docking.h"
#include "shared/components/Facility.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

using sr::Destroyed;
using sr::Docked;
using sr::DockingBay;
using sr::DockPrompt;
using sr::DockRequest;
using sr::FacilityKind;
using sr::FacilityRef;
using sr::FactionId;
using sr::FactionRef;
using sr::Health;
using sr::ParentRig;
using sr::PlayerLocation;
using sr::Rig;
using sr::Targetable;
using sr::ThrustInput;
using sr::UndockRequest;
using sr::Vec2;
using sr::Velocity;
using sr::WorldTransform;
using sr::core::diplomacy::DiplomacyMatrix;
using sr::core::diplomacy::Relation;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace docking_system = sr::space::docking_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, DiplomacyMatrix& diplomacy,
                          float dt = 1.0f / 60.0f) {
    SystemContext ctx{world, intents, content, dt, 0};
    ctx.diplomacy = &diplomacy;
    return ctx;
}

entt::entity MakeShip(entt::registry& registry, const Vec2& position, const char* faction) {
    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, position, 0.0f);
    registry.emplace<FactionRef>(root, FactionId(faction));
    registry.emplace<Targetable>(root);
    registry.emplace<Velocity>(root, Vec2{100.0f, 0.0f}, 0.0f);
    registry.emplace<ThrustInput>(root, 1.0f, 0.0f, 0.0f);
    return root;
}

// A station root with one docking-bay hardpoint at `bayPosition`.
entt::entity MakeStation(entt::registry& registry, const Vec2& stationPosition,
                         const Vec2& bayPosition, const char* faction, entt::entity& outBay) {
    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, stationPosition, 0.0f);
    registry.emplace<FactionRef>(root, FactionId(faction));

    const entt::entity bay = registry.create();
    registry.emplace<ParentRig>(bay, root);
    registry.emplace<WorldTransform>(bay, bayPosition, 0.0f);
    registry.emplace<DockingBay>(bay);
    outBay = bay;

    return root;
}

}  // namespace

TEST_CASE("DockingSystem prompts a Targetable rig within range of a same-faction bay",
          "[docking]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    entt::entity bay = entt::null;
    MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "aegis", bay);
    const entt::entity ship = MakeShip(registry, Vec2{20.0f, 0.0f}, "aegis");

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    REQUIRE(registry.all_of<DockPrompt>(ship));
    CHECK(registry.get<DockPrompt>(ship).bay == bay);
}

TEST_CASE("DockingSystem prompts a Targetable rig at a Neutral, non-owned station's bay",
          "[docking]") {
    // features.md 5.3's band table, applied: Neutral is not Distrustful-or-below, so docking at
    // a station you do not own is possible at all -- what the whole Market screen assumes
    // (architecture.md 13.3 finding N / 12.30.3). "kore" vs "aegis" is left unset -- Neutral.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    entt::entity bay = entt::null;
    MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "kore", bay);
    const entt::entity ship = MakeShip(registry, Vec2{20.0f, 0.0f}, "aegis");

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    REQUIRE(registry.all_of<DockPrompt>(ship));
    CHECK(registry.get<DockPrompt>(ship).bay == bay);
}

TEST_CASE("DockingSystem prompts a Targetable rig at an Allied station's bay", "[docking]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("pyre"), Relation::Allied);

    entt::entity bay = entt::null;
    MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "pyre", bay);
    const entt::entity ship = MakeShip(registry, Vec2{20.0f, 0.0f}, "aegis");

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    REQUIRE(registry.all_of<DockPrompt>(ship));
    CHECK(registry.get<DockPrompt>(ship).bay == bay);
}

TEST_CASE("DockingSystem does not prompt a rig out of range", "[docking]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    entt::entity bay = entt::null;
    MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "aegis", bay);
    const entt::entity ship = MakeShip(registry, Vec2{9000.0f, 0.0f}, "aegis");

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    CHECK_FALSE(registry.all_of<DockPrompt>(ship));
}

TEST_CASE("DockingSystem does not prompt a rig near a Hostile-faction bay", "[docking]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Hostile);

    entt::entity bay = entt::null;
    MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "reavers", bay);
    const entt::entity ship = MakeShip(registry, Vec2{20.0f, 0.0f}, "aegis");

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    CHECK_FALSE(registry.all_of<DockPrompt>(ship));
}

TEST_CASE("DockingSystem does not prompt a rig near a Distrustful-faction bay", "[docking]") {
    // Regression for the six-state Relation widening (architecture.md 12.32): Distrustful is its
    // own band, milder than Hostile, and the docking gate must refuse it too (features.md 5.3).
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;
    diplomacy.Set(FactionId("aegis"), FactionId("reavers"), Relation::Distrustful);

    entt::entity bay = entt::null;
    MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "reavers", bay);
    const entt::entity ship = MakeShip(registry, Vec2{20.0f, 0.0f}, "aegis");

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    CHECK_FALSE(registry.all_of<DockPrompt>(ship));
}

TEST_CASE("DockingSystem docks a rig whose DockRequest names the prompted bay", "[docking]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    entt::entity bay = entt::null;
    const entt::entity station =
        MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "aegis", bay);
    const entt::entity ship = MakeShip(registry, Vec2{20.0f, 0.0f}, "aegis");
    registry.emplace<DockRequest>(ship, bay);

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    REQUIRE(registry.all_of<Docked>(ship));
    CHECK(registry.get<Docked>(ship).station == station);
    CHECK(registry.get<Docked>(ship).bay == bay);
    CHECK_FALSE(registry.all_of<Targetable>(ship));
    CHECK_FALSE(registry.all_of<DockRequest>(ship));
}

TEST_CASE("DockingSystem drops a DockRequest naming a bay that is not in range", "[docking]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    entt::entity farBay = entt::null;
    MakeStation(registry, Vec2{9000.0f, 0.0f}, Vec2{9000.0f, 0.0f}, "aegis", farBay);
    const entt::entity ship = MakeShip(registry, Vec2{0.0f, 0.0f}, "aegis");
    registry.emplace<DockRequest>(ship, farBay);

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    CHECK_FALSE(registry.all_of<Docked>(ship));
    CHECK_FALSE(registry.all_of<DockRequest>(ship));
}

TEST_CASE(
    "DockingSystem does not heal a docked rig -- that moved to the paid, facility-gated "
    "Repair path (StationServicesSystem, architecture.md 13.3 finding I)",
    "[docking]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    const entt::entity station = registry.create();
    const entt::entity bay = registry.create();
    registry.emplace<ParentRig>(bay, station);
    registry.emplace<WorldTransform>(bay, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<DockingBay>(bay);

    const entt::entity ship = registry.create();
    registry.emplace<Docked>(ship, station, bay);

    Rig rig;
    const entt::entity living = registry.create();
    registry.emplace<Health>(living, 50.0f, 100.0f);
    rig.children.push_back(living);
    registry.emplace<Rig>(ship, rig);

    docking_system::Tick(MakeContext(world, intents, content, diplomacy, 1.0f));

    CHECK(registry.get<Health>(living).current == Catch::Approx(50.0f));
}

TEST_CASE("DockingSystem zeroes a docked rig's Velocity and ThrustInput", "[docking]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    const entt::entity station = registry.create();
    const entt::entity ship = registry.create();
    registry.emplace<Docked>(ship, station, entt::null);
    registry.emplace<Rig>(ship);
    registry.emplace<Velocity>(ship, Vec2{500.0f, -20.0f}, 3.0f);
    registry.emplace<ThrustInput>(ship, 1.0f, -1.0f, 0.5f);

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    const Velocity& velocity = registry.get<Velocity>(ship);
    CHECK(velocity.linear.x == 0.0f);
    CHECK(velocity.linear.y == 0.0f);
    CHECK(velocity.angular == 0.0f);
    const ThrustInput& thrust = registry.get<ThrustInput>(ship);
    CHECK(thrust.forward == 0.0f);
    CHECK(thrust.strafe == 0.0f);
    CHECK(thrust.turn == 0.0f);
}

TEST_CASE("DockingSystem undocks a rig with an UndockRequest and restores Targetable",
          "[docking]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    const entt::entity station = registry.create();
    const entt::entity ship = registry.create();
    registry.emplace<Docked>(ship, station, entt::null);
    registry.emplace<Rig>(ship);
    registry.emplace<UndockRequest>(ship);

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    CHECK_FALSE(registry.all_of<Docked>(ship));
    CHECK_FALSE(registry.all_of<UndockRequest>(ship));
    CHECK(registry.all_of<Targetable>(ship));
}

TEST_CASE("DockingSystem does not prompt or dock a rig at a destroyed bay", "[docking]") {
    // Regression: found during #133's M1 verification pass -- FindEligibleBay's view had no
    // exclude<Destroyed>, so a wrecked bay (DamageSystem only tags Destroyed, it never strips
    // DockingBay/ParentRig/WorldTransform) kept prompting "[R] DOCK" and would actually dock the
    // player onto it.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    entt::entity bay = entt::null;
    MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "aegis", bay);
    registry.emplace<Destroyed>(bay);
    const entt::entity ship = MakeShip(registry, Vec2{20.0f, 0.0f}, "aegis");
    registry.emplace<DockRequest>(ship, bay);

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    CHECK_FALSE(registry.all_of<DockPrompt>(ship));
    CHECK_FALSE(registry.all_of<Docked>(ship));
}

TEST_CASE("DockingSystem never docks a rig with its own bay", "[docking]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    // A station is itself a rig root with FactionRef + Targetable, so it is also a candidate
    // "docker" -- it must never resolve its own bay as an eligible target.
    const entt::entity station = registry.create();
    registry.emplace<WorldTransform>(station, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<FactionRef>(station, FactionId("aegis"));
    registry.emplace<Targetable>(station);

    const entt::entity bay = registry.create();
    registry.emplace<ParentRig>(bay, station);
    registry.emplace<WorldTransform>(bay, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<DockingBay>(bay);

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    CHECK_FALSE(registry.all_of<DockPrompt>(station));
}

TEST_CASE("DockingSystem does not prompt a rig toward a bay already at capacity", "[docking]") {
    // architecture.md 12.30.2: "a full bay is not an eligible bay."
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    entt::entity bay = entt::null;
    MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "aegis", bay);
    registry.emplace<FacilityRef>(bay, FacilityKind::Docking, /*grade=*/1, /*capacity=*/1);

    const entt::entity docked = registry.create();
    registry.emplace<Docked>(docked, entt::null, bay);

    const entt::entity ship = MakeShip(registry, Vec2{20.0f, 0.0f}, "aegis");

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    CHECK_FALSE(registry.all_of<DockPrompt>(ship));
}

TEST_CASE("DockingSystem prompts again once a full bay's docked vessel has launched",
          "[docking]") {
    // Occupancy is counted at the top of the tick a launch is processed in, so the bay reads
    // full through that same tick (Tick() runs UpdatePromptsAndRequests, which snapshots
    // occupancy, before ImmobilizeDocked consumes the UndockRequest) -- capacity frees up as of
    // the next tick, the same one-tick lag PlayerControlled's own derivation already accepts.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    entt::entity bay = entt::null;
    MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "aegis", bay);
    registry.emplace<FacilityRef>(bay, FacilityKind::Docking, /*grade=*/1, /*capacity=*/1);

    const entt::entity docked = registry.create();
    registry.emplace<Docked>(docked, entt::null, bay);
    registry.emplace<UndockRequest>(docked);
    registry.emplace<Rig>(docked);

    const entt::entity ship = MakeShip(registry, Vec2{20.0f, 0.0f}, "aegis");

    const SystemContext ctx = MakeContext(world, intents, content, diplomacy);
    docking_system::Tick(ctx);
    CHECK_FALSE(registry.all_of<Docked>(docked));  // The launch itself did land this tick.

    docking_system::Tick(ctx);
    REQUIRE(registry.all_of<DockPrompt>(ship));
    CHECK(registry.get<DockPrompt>(ship).bay == bay);
}

TEST_CASE("DockingSystem's capacity: 0 never blocks", "[docking]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    entt::entity bay = entt::null;
    MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "aegis", bay);
    registry.emplace<FacilityRef>(bay, FacilityKind::Docking, /*grade=*/1, /*capacity=*/0);

    for (int i = 0; i < 3; ++i) {
        const entt::entity docked = registry.create();
        registry.emplace<Docked>(docked, entt::null, bay);
    }

    const entt::entity ship = MakeShip(registry, Vec2{20.0f, 0.0f}, "aegis");

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    CHECK(registry.all_of<DockPrompt>(ship));
}

TEST_CASE("DockingSystem moves the player's PlayerLocation onto Docked.bay on arrival",
          "[docking]") {
    // architecture.md 12.30.2: "PlayerLocation resolves to Docked.bay on arrival" -- the Bay
    // screen is the default screen a player lands on, without clicking a router tab first.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    entt::entity bay = entt::null;
    MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "aegis", bay);
    const entt::entity ship = MakeShip(registry, Vec2{20.0f, 0.0f}, "aegis");
    registry.emplace<PlayerLocation>(ship, PlayerLocation{ship});
    registry.emplace<DockRequest>(ship, bay);

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    REQUIRE(registry.all_of<Docked>(ship));
    CHECK_FALSE(registry.all_of<PlayerLocation>(ship));
    REQUIRE(registry.all_of<PlayerLocation>(bay));
    CHECK(registry.get<PlayerLocation>(bay).shell == bay);
}

TEST_CASE("DockingSystem leaves a non-player rig's own location untouched on dock", "[docking]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;
    DiplomacyMatrix diplomacy;

    entt::entity bay = entt::null;
    MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "aegis", bay);
    const entt::entity ship = MakeShip(registry, Vec2{20.0f, 0.0f}, "aegis");
    registry.emplace<DockRequest>(ship, bay);
    // No PlayerLocation entity anywhere -- an NPC dock must not fabricate one.

    docking_system::Tick(MakeContext(world, intents, content, diplomacy));

    REQUIRE(registry.all_of<Docked>(ship));
    CHECK_FALSE(registry.all_of<PlayerLocation>(ship));
    CHECK_FALSE(registry.all_of<PlayerLocation>(bay));
}

TEST_CASE("DockingSystem fails closed with no diplomacy pointer wired", "[docking]") {
    // architecture.md 12.24 step 6: ctx.diplomacy is nullptr until wired. Nothing is dockable in
    // that state, not even a same-faction bay -- the same convention TemplateMarketSystem and
    // TargetingSystem use.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    entt::entity bay = entt::null;
    MakeStation(registry, Vec2{0.0f, 0.0f}, Vec2{0.0f, 0.0f}, "aegis", bay);
    const entt::entity ship = MakeShip(registry, Vec2{20.0f, 0.0f}, "aegis");

    SystemContext ctx{world, intents, content, 1.0f / 60.0f, 0};  // ctx.diplomacy left nullptr.
    docking_system::Tick(ctx);

    CHECK_FALSE(registry.all_of<DockPrompt>(ship));
}
