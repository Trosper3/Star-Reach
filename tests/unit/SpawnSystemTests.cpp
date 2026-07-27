#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/SpawnSystem.h"
#include "shared/components/Identity.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Spawn.h"
#include "shared/components/Transform.h"

using Catch::Approx;
using sr::CollisionRadius;
using sr::Distance;
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

    spawn_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.all_of<RespawnPending>(respawning));
}
