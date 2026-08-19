#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/SpawnSystem.h"
#include "shared/components/Docking.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Spawn.h"
#include "shared/components/Transform.h"

using Catch::Approx;
using sr::CollisionRadius;
using sr::Distance;
using sr::DockingBay;
using sr::FactionId;
using sr::FactionRef;
using sr::ParentRig;
using sr::PlayerControlled;
using sr::RespawnPending;
using sr::Rig;
using sr::SpawnAnchor;
using sr::Vec2;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace spawn_system = sr::space::spawn_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

entt::entity MakeAnchor(entt::registry& registry, const Vec2& position) {
    const entt::entity anchor = registry.create();
    registry.emplace<WorldTransform>(anchor, position, 0.0f);
    registry.emplace<SpawnAnchor>(anchor);
    return anchor;
}

// A rig root with one hardpoint child, matching how RigFactory shapes a real rig closely enough
// for culling's teardown to be exercised.
entt::entity MakeRig(entt::registry& registry, const Vec2& position) {
    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, position, 0.0f);
    Rig rig;
    const entt::entity hardpoint = registry.create();
    rig.children.push_back(hardpoint);
    registry.emplace<Rig>(root, rig);
    return root;
}

// A station root of `faction` with one DockingBay hardpoint offset from it by `bayLocalOffset` --
// enough of RigFactory's real shape (ParentRig, WorldTransform) for FindNearestFriendlyBayExit to
// resolve against, without pulling in content or a full rig build.
entt::entity MakeStationWithBay(entt::registry& registry, const Vec2& stationPosition,
                                const Vec2& bayLocalOffset, const FactionId& faction) {
    const entt::entity station = registry.create();
    registry.emplace<WorldTransform>(station, stationPosition, 0.0f);
    registry.emplace<FactionRef>(station, faction);

    const entt::entity bay = registry.create();
    registry.emplace<WorldTransform>(bay, stationPosition + bayLocalOffset, 0.0f);
    registry.emplace<ParentRig>(bay, station);
    registry.emplace<DockingBay>(bay);
    return station;
}

}  // namespace

TEST_CASE("SpawnSystem culls a rig far from every anchor", "[spawn]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    MakeAnchor(registry, Vec2{0.0f, 0.0f});
    const entt::entity far = MakeRig(registry, Vec2{50000.0f, 0.0f});
    const entt::entity hardpoint = registry.get<Rig>(far).children.front();

    spawn_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.valid(far));
    CHECK_FALSE(registry.valid(hardpoint));
}

TEST_CASE("SpawnSystem leaves a rig near an anchor alone", "[spawn]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    MakeAnchor(registry, Vec2{0.0f, 0.0f});
    const entt::entity near = MakeRig(registry, Vec2{500.0f, 0.0f});

    spawn_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(near));
}

TEST_CASE("SpawnSystem never culls a PlayerControlled rig regardless of distance", "[spawn]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    MakeAnchor(registry, Vec2{0.0f, 0.0f});
    const entt::entity player = MakeRig(registry, Vec2{50000.0f, 0.0f});
    registry.emplace<PlayerControlled>(player);

    spawn_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(player));
}

TEST_CASE("SpawnSystem culls nothing when the registry has no SpawnAnchor at all", "[spawn]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity lonely = MakeRig(registry, Vec2{50000.0f, 0.0f});

    spawn_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.valid(lonely));
}

TEST_CASE("SpawnSystem places a respawning rig near the nearest anchor and clears the tag",
          "[spawn]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity anchor = MakeAnchor(registry, Vec2{1000.0f, 0.0f});
    const entt::entity respawning = registry.create();
    registry.emplace<WorldTransform>(respawning, Vec2{-9000.0f, -9000.0f}, 0.0f);
    registry.emplace<RespawnPending>(respawning, 80.0f);
    registry.emplace<FactionRef>(respawning, FactionId("test_faction"));

    spawn_system::Tick(MakeContext(world, intents, content));

    const Vec2& anchorPos = registry.get<WorldTransform>(anchor).position;
    const Vec2& resultPos = registry.get<WorldTransform>(respawning).position;
    // First ring is 150 units out (SpawnSystem.cpp's kRingSpacingUnits), nothing else to avoid.
    CHECK(Distance(anchorPos, resultPos) < 200.0f);
    CHECK_FALSE(registry.all_of<RespawnPending>(respawning));
}

TEST_CASE("SpawnSystem picks a placement clear of a rig already sitting next to the anchor",
          "[spawn]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity anchor = MakeAnchor(registry, Vec2{0.0f, 0.0f});
    // A large blocker sitting exactly where the first ring's first attempt would land.
    const entt::entity blocker = registry.create();
    registry.emplace<WorldTransform>(blocker, Vec2{150.0f, 0.0f}, 0.0f);
    registry.emplace<CollisionRadius>(blocker, 100.0f);

    const entt::entity respawning = registry.create();
    registry.emplace<WorldTransform>(respawning, Vec2{-9000.0f, -9000.0f}, 0.0f);
    registry.emplace<RespawnPending>(respawning, 80.0f);
    registry.emplace<FactionRef>(respawning, FactionId("test_faction"));

    spawn_system::Tick(MakeContext(world, intents, content));

    const Vec2& anchorPos = registry.get<WorldTransform>(anchor).position;
    const Vec2& blockerPos = registry.get<WorldTransform>(blocker).position;
    const Vec2& resultPos = registry.get<WorldTransform>(respawning).position;
    CHECK(Distance(blockerPos, resultPos) >= 180.0f);  // blocker radius (100) + margin (80).
    CHECK(Distance(anchorPos, resultPos) < 400.0f);    // Still close to the anchor, not exiled.
}

TEST_CASE("SpawnSystem leaves a respawn request untouched when no anchor exists yet", "[spawn]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity respawning = registry.create();
    registry.emplace<WorldTransform>(respawning, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<RespawnPending>(respawning, 80.0f);
    registry.emplace<FactionRef>(respawning, FactionId("test_faction"));

    spawn_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.all_of<RespawnPending>(respawning));
}

TEST_CASE("SpawnSystem respawns a rig at its friendly station's DockingBay exit, not the anchor",
          "[spawn]") {
    // architecture.md 12.36 / issue #160: exiting the bay takes priority over the bare
    // anchor-ring search this test's anchor would otherwise resolve to.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const FactionId faction("aegis_directorate");
    MakeAnchor(registry, Vec2{0.0f, 0.0f});
    const entt::entity station =
        MakeStationWithBay(registry, Vec2{2000.0f, 0.0f}, Vec2{30.0f, 0.0f}, faction);
    const Vec2 bayPos = registry.get<WorldTransform>(station).position + Vec2{30.0f, 0.0f};

    const entt::entity respawning = registry.create();
    registry.emplace<WorldTransform>(respawning, Vec2{-9000.0f, -9000.0f}, 0.0f);
    registry.emplace<RespawnPending>(respawning, 80.0f);
    registry.emplace<FactionRef>(respawning, faction);

    spawn_system::Tick(MakeContext(world, intents, content));

    const Vec2& resultPos = registry.get<WorldTransform>(respawning).position;
    CHECK(Distance(bayPos, resultPos) < 100.0f);  // Clear exit point, no ring search needed.
    CHECK(Distance(resultPos, Vec2{0.0f, 0.0f}) > 1000.0f);  // Nowhere near the (unrelated) anchor.
    CHECK_FALSE(registry.all_of<RespawnPending>(respawning));
}

TEST_CASE("SpawnSystem falls back to the anchor when no friendly DockingBay exists", "[spawn]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    MakeAnchor(registry, Vec2{1000.0f, 0.0f});
    // A bay exists, but for a different faction -- must not be treated as friendly.
    MakeStationWithBay(registry, Vec2{5000.0f, 5000.0f}, Vec2{30.0f, 0.0f},
                       FactionId("the_forgotten"));

    const entt::entity respawning = registry.create();
    registry.emplace<WorldTransform>(respawning, Vec2{-9000.0f, -9000.0f}, 0.0f);
    registry.emplace<RespawnPending>(respawning, 80.0f);
    registry.emplace<FactionRef>(respawning, FactionId("aegis_directorate"));

    spawn_system::Tick(MakeContext(world, intents, content));

    const Vec2& resultPos = registry.get<WorldTransform>(respawning).position;
    CHECK(Distance(Vec2{1000.0f, 0.0f}, resultPos) < 200.0f);
    CHECK_FALSE(registry.all_of<RespawnPending>(respawning));
}
