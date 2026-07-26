#include <catch2/catch_test_macros.hpp>

#include "modes/space/data/SystemWorld.h"
#include "shared/components/Health.h"
#include "shared/components/Transform.h"

using sr::space::kInvalidNetworkId;
using sr::space::SystemWorld;

TEST_CASE("A tracked entity round-trips through its network id", "[world]") {
    SystemWorld world("sol", "Sol");
    const entt::entity entity = world.Registry().create();

    const auto id = world.Track(entity);
    REQUIRE(id != kInvalidNetworkId);
    CHECK((world.Resolve(id) == entity));
    CHECK(world.IdOf(entity) == id);
}

TEST_CASE("Tracking the same entity twice returns the same id", "[world]") {
    SystemWorld world("sol");
    const entt::entity entity = world.Registry().create();
    CHECK(world.Track(entity) == world.Track(entity));
}

TEST_CASE("An unknown network id resolves to null rather than to something else", "[world]") {
    SystemWorld world("sol");
    CHECK((world.Resolve(9999) == entt::null));
    CHECK(world.IdOf(entt::null) == kInvalidNetworkId);
}

TEST_CASE("A destroyed entity's id resolves to null, not to a recycled slot", "[world]") {
    // EnTT recycles entity slots. Without the validity check in Resolve, a stale id from a
    // save file or a late network packet would silently address whatever object took the slot
    // -- which is the precise failure Law 2's stable-id rule exists to prevent.
    SystemWorld world("sol");
    const entt::entity entity = world.Registry().create();
    const auto id = world.Track(entity);

    world.Registry().destroy(entity);
    CHECK((world.Resolve(id) == entt::null));
}

TEST_CASE("Forget releases the mapping in both directions", "[world]") {
    SystemWorld world("sol");
    const entt::entity entity = world.Registry().create();
    const auto id = world.Track(entity);

    world.Forget(id);
    CHECK((world.Resolve(id) == entt::null));
    CHECK(world.IdOf(entity) == kInvalidNetworkId);
}

TEST_CASE("Each world owns its own registry", "[world]") {
    // Law 2 -- there is no global entity store. Two systems' entities are unrelated, and an
    // entity handle from one is meaningless in the other.
    SystemWorld a("sol");
    SystemWorld b("alpha_centauri");

    const entt::entity inA = a.Registry().create();
    a.Registry().emplace<sr::Health>(inA, 100.0f, 100.0f);

    CHECK(a.Registry().storage<sr::Health>().size() == 1);
    CHECK(b.Registry().storage<sr::Health>().size() == 0);
    CHECK(a.SystemId() != b.SystemId());
}

TEST_CASE("Components attach and read back", "[world][components]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    const entt::entity rig = registry.create();
    registry.emplace<sr::WorldTransform>(rig, sr::Vec2{10.0f, -4.0f}, 1.5f);

    const auto& transform = registry.get<sr::WorldTransform>(rig);
    CHECK(transform.position.x == 10.0f);
    CHECK(transform.position.y == -4.0f);
    CHECK(transform.rotation == 1.5f);
}
