#include <catch2/catch_test_macros.hpp>

#include "modes/space/data/SystemWorld.h"
#include "modes/space/factories/WorldGen.h"
#include "shared/components/Commander.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"

using sr::Commander;
using sr::CrewRating;
using sr::Destroyed;
using sr::FactionId;
using sr::FactionRef;
using sr::Rig;
using sr::space::SystemWorld;
namespace world_gen = sr::space::world_gen;

namespace {

// One faction's minimal rig: a root with a single Bridge/cockpit-shaped hardpoint already
// carrying an officer -- the same shape every authored ship/station blueprint mounts today
// (data/base_game/ships.json's crew_officer_i on every cockpit/bridge mount), which is what makes
// "not a separate acquisition track" true: SeedCommanders never equips anything itself.
entt::entity MakeCrewedRig(entt::registry& registry, const char* faction, float commandRoll) {
    const entt::entity officer = registry.create();
    registry.emplace<CrewRating>(officer, 0.1f, commandRoll, 0.1f, 0.1f);

    const entt::entity root = registry.create();
    registry.emplace<FactionRef>(root, FactionId(faction));
    registry.emplace<Rig>(root, std::vector<entt::entity>{officer});
    return root;
}

}  // namespace

TEST_CASE("SeedCommanders promotes an eligible officer to Commander", "[world_gen][commander]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    const entt::entity root = MakeCrewedRig(registry, "aegis", 0.1f);
    const entt::entity officer = registry.get<Rig>(root).children[0];

    world_gen::SeedCommanders(registry);

    REQUIRE(registry.all_of<Commander>(officer));
    CHECK(registry.get<Commander>(officer).faction == FactionId("aegis"));
}

TEST_CASE("SeedCommanders caps at three commanders per faction", "[world_gen][commander]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    std::vector<entt::entity> officers;
    for (int i = 0; i < 5; ++i) {
        const entt::entity root = MakeCrewedRig(registry, "aegis", 0.1f);
        officers.push_back(registry.get<Rig>(root).children[0]);
    }

    world_gen::SeedCommanders(registry);

    int commanderCount = 0;
    for (const entt::entity officer : officers) {
        if (registry.all_of<Commander>(officer)) {
            ++commanderCount;
        }
    }
    CHECK(commanderCount == 3);
}

TEST_CASE("SeedCommanders seeds up to three commanders independently per faction",
          "[world_gen][commander]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    for (int i = 0; i < 3; ++i) {
        MakeCrewedRig(registry, "aegis", 0.1f);
    }
    for (int i = 0; i < 3; ++i) {
        MakeCrewedRig(registry, "the_forgotten", 0.1f);
    }

    world_gen::SeedCommanders(registry);

    int aegisCommanders = 0;
    int forgottenCommanders = 0;
    for (auto [hardpoint, commander] : registry.view<Commander>().each()) {
        if (commander.faction == FactionId("aegis")) {
            ++aegisCommanders;
        } else if (commander.faction == FactionId("the_forgotten")) {
            ++forgottenCommanders;
        }
    }
    CHECK(aegisCommanders == 3);
    CHECK(forgottenCommanders == 3);
}

TEST_CASE("SeedCommanders skips an officer with no command roll", "[world_gen][commander]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    const entt::entity root = MakeCrewedRig(registry, "aegis", 0.0f);
    const entt::entity officer = registry.get<Rig>(root).children[0];

    world_gen::SeedCommanders(registry);

    CHECK_FALSE(registry.all_of<Commander>(officer));
}

TEST_CASE("SeedCommanders skips a destroyed hardpoint", "[world_gen][commander]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    const entt::entity root = MakeCrewedRig(registry, "aegis", 0.1f);
    const entt::entity officer = registry.get<Rig>(root).children[0];
    registry.emplace<Destroyed>(officer);

    world_gen::SeedCommanders(registry);

    CHECK_FALSE(registry.all_of<Commander>(officer));
}

TEST_CASE("SeedCommanders is idempotent -- a second pass does not double-count toward the cap",
          "[world_gen][commander]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();

    for (int i = 0; i < 3; ++i) {
        MakeCrewedRig(registry, "aegis", 0.1f);
    }
    // A fourth arrives after the first seeding pass -- e.g. a later WorldGen call in the same
    // registry. It should not be promoted: the cap already holds three, and re-running must not
    // re-seed the first three either.
    world_gen::SeedCommanders(registry);
    const entt::entity late = MakeCrewedRig(registry, "aegis", 0.1f);
    const entt::entity lateOfficer = registry.get<Rig>(late).children[0];

    world_gen::SeedCommanders(registry);

    CHECK_FALSE(registry.all_of<Commander>(lateOfficer));
    int commanderCount = 0;
    for (auto [hardpoint, commander] : registry.view<Commander>().each()) {
        (void)hardpoint;
        (void)commander;
        ++commanderCount;
    }
    CHECK(commanderCount == 3);
}
