#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <entt/entity/registry.hpp>

#include "modes/space/ui/StatusProjection.h"
#include "shared/components/Health.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"
#include "shared/math/Vec2.h"

using Catch::Approx;
using sr::CollisionRadius;
using sr::Destroyed;
using sr::Health;
using sr::LocalTransform;
using sr::Rig;
using sr::ShellKind;
using sr::ShellRole;
using sr::Shield;
using sr::ShieldCoverage;
using sr::StructuralAttachment;
using sr::Vec2;
using sr::WorldTransform;
using sr::space::ui::status_projection::Build;
using sr::space::ui::status_projection::ChooseDetailLevel;
using sr::space::ui::status_projection::DetailLevel;

namespace {

entt::entity MakeHardpoint(entt::registry& registry, Vec2 offset, float healthCurrent,
                           float healthMax, ShellKind kind = ShellKind::Armor) {
    const entt::entity entity = registry.create();
    registry.emplace<LocalTransform>(entity, offset, 0.0f);
    registry.emplace<WorldTransform>(entity, offset, 0.0f);
    registry.emplace<Health>(entity, healthCurrent, healthMax);
    registry.emplace<ShellRole>(entity, kind);
    return entity;
}

}  // namespace

TEST_CASE("ChooseDetailLevel is Marker for a rig with no children", "[status-projection]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<Rig>(root);

    CHECK(ChooseDetailLevel(registry, root, 140.0f) == DetailLevel::Marker);
}

TEST_CASE("ChooseDetailLevel is Full for a small, well-spaced fighter rig", "[status-projection]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<CollisionRadius>(root, 20.0f);

    Rig rig;
    rig.children.push_back(MakeHardpoint(registry, Vec2{0.0f, 0.0f}, 50.0f, 50.0f));
    rig.children.push_back(MakeHardpoint(registry, Vec2{-18.0f, 0.0f}, 50.0f, 50.0f));
    rig.children.push_back(MakeHardpoint(registry, Vec2{8.5f, -14.7f}, 50.0f, 50.0f));
    rig.children.push_back(MakeHardpoint(registry, Vec2{8.5f, 14.7f}, 50.0f, 50.0f));
    registry.emplace<Rig>(root, std::move(rig));

    CHECK(ChooseDetailLevel(registry, root, 140.0f) == DetailLevel::Full);
}

TEST_CASE("ChooseDetailLevel collapses when hardpoints packed into a tiny diameter would overlap",
          "[status-projection]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<CollisionRadius>(root, 20.0f);

    Rig rig;
    for (int i = 0; i < 6; ++i) {
        const float angle = static_cast<float>(i);
        rig.children.push_back(MakeHardpoint(registry, Vec2{angle, angle * 0.5f}, 50.0f, 50.0f));
    }
    registry.emplace<Rig>(root, std::move(rig));

    // A 4px diameter cannot fit six legible 5px-radius dots no matter how tightly they're packed.
    CHECK(ChooseDetailLevel(registry, root, 4.0f) != DetailLevel::Full);
}

TEST_CASE("Build forces a Destroyed hardpoint's integrity to 0 despite a stale Health::current",
          "[status-projection]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<CollisionRadius>(root, 20.0f);

    Rig rig;
    const entt::entity cascaded =
        MakeHardpoint(registry, Vec2{-18.0f, 0.0f}, 40.0f, 50.0f);  // Never zeroed by the cascade.
    registry.emplace<Destroyed>(cascaded);
    rig.children.push_back(cascaded);
    rig.children.push_back(MakeHardpoint(registry, Vec2{18.0f, 0.0f}, 50.0f, 50.0f));
    registry.emplace<Rig>(root, std::move(rig));

    const auto projection = Build(registry, root, 140.0f);

    REQUIRE(projection.level == DetailLevel::Full);
    REQUIRE(projection.hardpoints.size() == 2);
    const auto destroyedNode =
        std::find_if(projection.hardpoints.begin(), projection.hardpoints.end(),
                     [](const auto& node) { return node.localOffset.x < 0.0f; });
    REQUIRE(destroyedNode != projection.hardpoints.end());
    CHECK(destroyedNode->integrity == Approx(0.0f));
}

TEST_CASE("Build's Personal shield loop encloses only its own hardpoint", "[status-projection]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<CollisionRadius>(root, 20.0f);

    Rig rig;
    const entt::entity emitter = MakeHardpoint(registry, Vec2{17.0f, 0.0f}, 30.0f, 30.0f);
    registry.emplace<Shield>(emitter, 80.0f, 100.0f, sr::DamageType::Energy, 0.0f, 0.0f, 0.0f,
                             ShieldCoverage::Personal, 0.0f);
    rig.children.push_back(emitter);
    rig.children.push_back(MakeHardpoint(registry, Vec2{-18.0f, 0.0f}, 50.0f, 50.0f));
    registry.emplace<Rig>(root, std::move(rig));

    const auto projection = Build(registry, root, 140.0f);

    REQUIRE(projection.loops.size() == 1);
    const auto& loop = projection.loops.front();
    CHECK(loop.localCenter.x == Approx(17.0f));
    CHECK(loop.localRadius == Approx(0.0f));  // Encloses only its own housing -- zero spread.
    CHECK(loop.chargeFraction == Approx(0.8f));
    CHECK(loop.absorbs == sr::DamageType::Energy);
}

TEST_CASE("Build's Conformal shield loop radius is the rig's own hull radius, not member spread",
          "[status-projection]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<CollisionRadius>(root, 25.0f);

    Rig rig;
    const entt::entity emitter = MakeHardpoint(registry, Vec2{0.0f, 0.0f}, 30.0f, 30.0f);
    registry.emplace<Shield>(emitter, 100.0f, 100.0f, sr::DamageType::Kinetic, 0.0f, 0.0f, 0.0f,
                             ShieldCoverage::Conformal, 0.0f);
    rig.children.push_back(emitter);
    rig.children.push_back(MakeHardpoint(registry, Vec2{-18.0f, 0.0f}, 50.0f, 50.0f));
    registry.emplace<Rig>(root, std::move(rig));

    const auto projection = Build(registry, root, 140.0f);

    REQUIRE(projection.loops.size() == 1);
    CHECK(projection.loops.front().localRadius == Approx(25.0f));
}

TEST_CASE("Build omits a loop for a Destroyed shield hardpoint", "[status-projection]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<CollisionRadius>(root, 20.0f);

    Rig rig;
    const entt::entity emitter = MakeHardpoint(registry, Vec2{17.0f, 0.0f}, 0.0f, 30.0f);
    registry.emplace<Shield>(emitter, 100.0f, 100.0f, sr::DamageType::Kinetic, 0.0f, 0.0f, 0.0f,
                             ShieldCoverage::Personal, 0.0f);
    registry.emplace<Destroyed>(emitter);
    rig.children.push_back(emitter);
    registry.emplace<Rig>(root, std::move(rig));

    const auto projection = Build(registry, root, 140.0f);

    CHECK(projection.loops.empty());
}

TEST_CASE("Build's Segments level aggregates a boundary hardpoint and its structural children",
          "[status-projection]") {
    entt::registry registry;
    const entt::entity root = registry.create();
    registry.emplace<CollisionRadius>(root, 20.0f);

    Rig rig;
    const entt::entity chassis =
        MakeHardpoint(registry, Vec2{0.0f, 0.0f}, 50.0f, 50.0f, ShellKind::Chassis);
    registry.emplace<StructuralAttachment>(chassis, entt::null);
    rig.children.push_back(chassis);

    const entt::entity turret =
        MakeHardpoint(registry, Vec2{0.01f, 0.01f}, 20.0f, 30.0f, ShellKind::Weapon);
    registry.emplace<StructuralAttachment>(turret, chassis);
    registry.emplace<Destroyed>(turret);  // Never zeroed by the cascade -- same trap as Health.h's.
    rig.children.push_back(turret);

    registry.emplace<Rig>(root, std::move(rig));

    // Force the collapse: the two hardpoints sit 0.014 world units apart, which no diameter this
    // small could ever draw as two legible, non-overlapping dots -- so both fold into the one
    // chassis-rooted segment, verified below.
    const auto projection = Build(registry, root, 0.5f);

    REQUIRE(projection.level == DetailLevel::Segments);
    REQUIRE(projection.segments.size() == 1);
    // (50 + 0) / (50 + 30) -- the turret's death drags the whole segment's aggregate down.
    CHECK(projection.segments.front().integrity == Approx(50.0f / 80.0f));
}
