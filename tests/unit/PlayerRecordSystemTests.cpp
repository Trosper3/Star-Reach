#include <catch2/catch_test_macros.hpp>

#include <entt/entity/registry.hpp>

#include "modes/space/systems/PlayerRecordSystem.h"
#include "shared/components/Identity.h"

using sr::FactionId;
using sr::PlayerFaction;
using sr::PlayerRecordSingleton;
namespace player_record_system = sr::space::player_record_system;

TEST_CASE("PlayerRecordSystem::FactionOf is empty before anything sets it",
          "[player-record-system]") {
    entt::registry registry;
    CHECK(player_record_system::FactionOf(registry).empty());
}

TEST_CASE("PlayerRecordSystem::SetFaction creates the record on first call",
          "[player-record-system]") {
    entt::registry registry;
    player_record_system::SetFaction(registry, FactionId("aegis"));

    CHECK(player_record_system::FactionOf(registry) == FactionId("aegis"));
    CHECK(registry.view<PlayerRecordSingleton>().size() == 1);
}

TEST_CASE("PlayerRecordSystem::SetFaction updates the existing record rather than duplicating it",
          "[player-record-system]") {
    entt::registry registry;
    player_record_system::SetFaction(registry, FactionId("aegis"));
    player_record_system::SetFaction(registry, FactionId("zenith"));

    CHECK(player_record_system::FactionOf(registry) == FactionId("zenith"));
    CHECK(registry.view<PlayerRecordSingleton>().size() == 1);
    CHECK(registry.view<PlayerFaction>().size() == 1);
}
