#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/ui/CommsPanel.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/components/Transform.h"

using sr::DisplayName;
using sr::Rig;
using sr::Vec2;
using sr::WorldTransform;
using sr::space::ui::comms_panel::NearestHailable;

namespace {

entt::entity MakeContact(entt::registry& registry, Vec2 position, const char* name) {
    const entt::entity entity = registry.create();
    registry.emplace<WorldTransform>(entity, position, 0.0f);
    registry.emplace<DisplayName>(entity, name);
    registry.emplace<Rig>(entity);
    return entity;
}

}  // namespace

TEST_CASE("NearestHailable is entt::null for entt::null", "[comms-panel]") {
    entt::registry registry;
    CHECK((NearestHailable(registry, entt::null) == entt::null));
}

TEST_CASE("NearestHailable is entt::null for a player with no WorldTransform", "[comms-panel]") {
    entt::registry registry;
    const entt::entity player = registry.create();
    CHECK((NearestHailable(registry, player) == entt::null));
}

TEST_CASE("NearestHailable is entt::null with no other named rig in the registry",
          "[comms-panel]") {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<WorldTransform>(player, Vec2{0.0f, 0.0f}, 0.0f);
    CHECK((NearestHailable(registry, player) == entt::null));
}

TEST_CASE("NearestHailable picks the closest named rig, not the first one created",
          "[comms-panel]") {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<WorldTransform>(player, Vec2{0.0f, 0.0f}, 0.0f);

    const entt::entity far = MakeContact(registry, Vec2{500.0f, 0.0f}, "Far Station");
    const entt::entity near = MakeContact(registry, Vec2{10.0f, 0.0f}, "Near Station");
    (void)far;

    CHECK(NearestHailable(registry, player) == near);
}

TEST_CASE("NearestHailable never names the player's own rig", "[comms-panel]") {
    entt::registry registry;
    const entt::entity player = MakeContact(registry, Vec2{0.0f, 0.0f}, "My Ship");
    CHECK((NearestHailable(registry, player) == entt::null));
}

TEST_CASE("NearestHailable skips a WorldTransform entity with no DisplayName or no Rig",
          "[comms-panel]") {
    entt::registry registry;
    const entt::entity player = registry.create();
    registry.emplace<WorldTransform>(player, Vec2{0.0f, 0.0f}, 0.0f);

    // A hardpoint: WorldTransform without Rig -- must not be treated as a hailable contact.
    const entt::entity hardpoint = registry.create();
    registry.emplace<WorldTransform>(hardpoint, Vec2{5.0f, 0.0f}, 0.0f);
    registry.emplace<DisplayName>(hardpoint, "Turret");

    // A wreck: WorldTransform without DisplayName.
    const entt::entity wreck = registry.create();
    registry.emplace<WorldTransform>(wreck, Vec2{6.0f, 0.0f}, 0.0f);
    registry.emplace<Rig>(wreck);

    const entt::entity station = MakeContact(registry, Vec2{50.0f, 0.0f}, "Outpost");

    CHECK(NearestHailable(registry, player) == station);
}
