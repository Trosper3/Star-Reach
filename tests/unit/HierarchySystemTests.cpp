#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <utility>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/HierarchySystem.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"

using Catch::Approx;
using sr::LocalTransform;
using sr::PreviousTransform;
using sr::Rig;
using sr::Vec2;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace hierarchy_system = sr::space::hierarchy_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

}  // namespace

TEST_CASE("HierarchySystem places a child at the root's position plus its rotated offset",
          "[hierarchy]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{10.0f, 0.0f}, sr::kPi / 2.0f);

    const entt::entity child = registry.create();
    registry.emplace<LocalTransform>(child, Vec2{5.0f, 0.0f}, 0.0f);
    registry.emplace<WorldTransform>(child);

    Rig rig;
    rig.children.push_back(child);
    registry.emplace<Rig>(root, std::move(rig));

    hierarchy_system::Tick(MakeContext(world, intents, content));

    const auto& childXf = registry.get<WorldTransform>(child);
    CHECK(childXf.position.x == Approx(10.0f).margin(0.0001f));
    CHECK(childXf.position.y == Approx(5.0f).margin(0.0001f));
    CHECK(childXf.rotation == Approx(sr::kPi / 2.0f));
}

TEST_CASE("HierarchySystem records the child's prior WorldTransform into PreviousTransform",
          "[hierarchy]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{0.0f, 0.0f}, 0.0f);

    const entt::entity child = registry.create();
    registry.emplace<LocalTransform>(child, Vec2{3.0f, 0.0f}, 0.0f);
    registry.emplace<WorldTransform>(child, Vec2{1.0f, 1.0f}, 0.0f);
    registry.emplace<PreviousTransform>(child, Vec2{1.0f, 1.0f}, 0.0f);

    Rig rig;
    rig.children.push_back(child);
    registry.emplace<Rig>(root, std::move(rig));

    hierarchy_system::Tick(MakeContext(world, intents, content));

    const auto& prev = registry.get<PreviousTransform>(child);
    CHECK(prev.position.x == Approx(1.0f));
    CHECK(prev.position.y == Approx(1.0f));
}

TEST_CASE("HierarchySystem skips rig children with no positional components", "[hierarchy]") {
    // A rig member without LocalTransform/WorldTransform is not a rendered hardpoint. The system
    // must not crash reaching for components that were never attached.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, Vec2{0.0f, 0.0f}, 0.0f);

    const entt::entity bareChild = registry.create();

    Rig rig;
    rig.children.push_back(bareChild);
    registry.emplace<Rig>(root, std::move(rig));

    CHECK_NOTHROW(hierarchy_system::Tick(MakeContext(world, intents, content)));
}
