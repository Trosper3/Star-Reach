#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/TutorialSystem.h"
#include "shared/blueprints/Ids.h"
#include "shared/components/Combat.h"
#include "shared/components/Comms.h"
#include "shared/components/Identity.h"
#include "shared/components/Party.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/components/Tutorial.h"
#include "shared/components/Warp.h"

using sr::AnomalyField;
using sr::CommsLog;
using sr::CommsLogSingleton;
using sr::DisplayName;
using sr::MountedModules;
using sr::ParentRig;
using sr::PartyLeader;
using sr::PlayerControlled;
using sr::Rig;
using sr::SystemWarpRequest;
using sr::Target;
using sr::Tutorial;
using sr::TutorialDeclined;
using sr::TutorialOffer;
using sr::TutorialStep;
using sr::Vec2;
using sr::Weapon;
using sr::WorldBody;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace tutorial_system = sr::space::tutorial_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, unsigned long long tick = 0) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, tick};
}

entt::entity MakeRig(entt::registry& registry, const Vec2& position, TutorialStep step) {
    const entt::entity self = registry.create();
    registry.emplace<WorldTransform>(self, position, 0.0f);
    registry.emplace<Rig>(self, std::vector<entt::entity>{});
    Tutorial tutorial;
    tutorial.step = step;
    registry.emplace<Tutorial>(self, tutorial);
    return self;
}

// A hardpoint entity, parented onto `root`'s Rig::children, so CountMountedModules/
// HasRecentlyFired can find it.
entt::entity AddHardpoint(entt::registry& registry, entt::entity root) {
    const entt::entity hardpoint = registry.create();
    registry.emplace<ParentRig>(hardpoint, root);
    registry.get<Rig>(root).children.push_back(hardpoint);
    return hardpoint;
}

entt::entity MakePlayer(entt::registry& registry, const Vec2& position) {
    const entt::entity self = registry.create();
    registry.emplace<WorldTransform>(self, position, 0.0f);
    registry.emplace<Rig>(self, std::vector<entt::entity>{});
    registry.emplace<PlayerControlled>(self);
    return self;
}

}  // namespace

TEST_CASE("TutorialSystem captures the starting position and equipped-module baseline",
          "[tutorial]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{100.0f, 50.0f}, TutorialStep::Move);
    const entt::entity hardpoint = AddHardpoint(registry, rig);
    registry.emplace<MountedModules>(hardpoint, std::vector<sr::ModuleId>{sr::ModuleId("weapon")});

    tutorial_system::Tick(MakeContext(world, intents, content));

    const Tutorial& tutorial = registry.get<Tutorial>(rig);
    CHECK(tutorial.started);
    CHECK(tutorial.startPosition.x == 100.0f);
    CHECK(tutorial.startPosition.y == 50.0f);
    CHECK(tutorial.startEquippedModules == 1);
}

TEST_CASE("TutorialSystem advances Move once the rig travels far enough", "[tutorial]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Move);
    tutorial_system::Tick(MakeContext(world, intents, content));  // Captures startPosition.

    registry.get<WorldTransform>(rig).position = Vec2{400.0f, 0.0f};
    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::Target);
}

TEST_CASE("TutorialSystem does not advance Move before the distance threshold", "[tutorial]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Move);
    tutorial_system::Tick(MakeContext(world, intents, content));

    registry.get<WorldTransform>(rig).position = Vec2{100.0f, 0.0f};
    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::Move);
}

TEST_CASE("TutorialSystem advances Target once the rig has a live Target", "[tutorial]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Target);
    const entt::entity someRig = registry.create();
    registry.emplace<Target>(rig, someRig, entt::null);

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::Fire);
}

TEST_CASE("TutorialSystem advances Fire once a weapon hardpoint is cooling down", "[tutorial]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Fire);
    const entt::entity hardpoint = AddHardpoint(registry, rig);
    Weapon weapon;
    weapon.fireIntervalSeconds = 1.0f;
    weapon.cooldown = 1.0f;  // WeaponSystem sets this to fireIntervalSeconds the tick it fires.
    registry.emplace<Weapon>(hardpoint, weapon);

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::Equip);
}

TEST_CASE("TutorialSystem does not advance Fire with every weapon at zero cooldown", "[tutorial]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Fire);
    const entt::entity hardpoint = AddHardpoint(registry, rig);
    registry.emplace<Weapon>(hardpoint, Weapon{});  // cooldown defaults to 0.0f.

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::Fire);
}

TEST_CASE("TutorialSystem advances Equip once mounted modules exceed the starting baseline",
          "[tutorial]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Equip);
    const entt::entity hardpoint = AddHardpoint(registry, rig);
    // Captures the baseline (zero -- no MountedModules yet) on this first tick.
    tutorial_system::Tick(MakeContext(world, intents, content));
    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::Equip);

    registry.emplace<MountedModules>(hardpoint, std::vector<sr::ModuleId>{sr::ModuleId("armor")});
    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::Done);
}

TEST_CASE("TutorialSystem does not advance Equip for modules already mounted at tutorial start",
          "[tutorial]") {
    // The baseline exists precisely so a blueprint's own factory-mounted modules don't complete
    // this step before the player has done anything (TutorialSystem.h's own doc comment).
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Equip);
    const entt::entity hardpoint = AddHardpoint(registry, rig);
    registry.emplace<MountedModules>(hardpoint, std::vector<sr::ModuleId>{sr::ModuleId("armor")});

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::Equip);
}

TEST_CASE("TutorialSystem never advances past Done", "[tutorial]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = MakeRig(registry, Vec2{0.0f, 0.0f}, TutorialStep::Done);

    for (int i = 0; i < 10; ++i) {
        tutorial_system::Tick(MakeContext(world, intents, content));
    }

    CHECK(registry.get<Tutorial>(rig).step == TutorialStep::Done);
}

TEST_CASE("TutorialSystem leaves a rig with no Tutorial component untouched", "[tutorial]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity rig = registry.create();
    registry.emplace<WorldTransform>(rig, Vec2{0.0f, 0.0f}, 0.0f);

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<Tutorial>(rig));
}

TEST_CASE("TutorialSystem offers instruction only when an AnomalyField is present", "[tutorial]") {
    SystemWorld world("sol");  // No AnomalyField -- a normal, procedurally generated system.
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f});

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<TutorialOffer>(player));
}

TEST_CASE("TutorialSystem offers instruction once, and logs the hail", "[tutorial]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity anomaly = registry.create();
    registry.emplace<WorldTransform>(anomaly, Vec2{5000.0f, 0.0f}, 0.0f);
    registry.emplace<AnomalyField>(anomaly, 400.0f);
    const entt::entity leader = registry.create();
    registry.emplace<DisplayName>(leader, "Vanguard");
    registry.emplace<PartyLeader>(leader, std::vector<entt::entity>{});
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f});

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.all_of<TutorialOffer>(player));
    const auto logs = registry.view<CommsLogSingleton, CommsLog>();
    REQUIRE(std::distance(logs.begin(), logs.end()) == 1);
    for (auto [entity, log] : logs.each()) {
        (void)entity;
        REQUIRE(log.entries.size() == 1);
        CHECK(log.entries.front().text.find("Vanguard") != std::string::npos);
    }

    // A second tick must not re-offer or log a second hail.
    tutorial_system::Tick(MakeContext(world, intents, content));
    for (auto [entity, log] : logs.each()) {
        (void)entity;
        CHECK(log.entries.size() == 1);
    }
}

TEST_CASE("TutorialSystem does not re-offer once declined", "[tutorial]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity anomaly = registry.create();
    registry.emplace<WorldTransform>(anomaly, Vec2{5000.0f, 0.0f}, 0.0f);
    registry.emplace<AnomalyField>(anomaly, 400.0f);
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f});
    registry.emplace<TutorialDeclined>(player);

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<TutorialOffer>(player));
}

TEST_CASE("TutorialSystem fires the cataclysm once the player crosses the trigger radius",
          "[tutorial][cataclysm]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity anomaly = registry.create();
    registry.emplace<WorldTransform>(anomaly, Vec2{5000.0f, 0.0f}, 0.0f);
    registry.emplace<AnomalyField>(anomaly, 400.0f);
    const entt::entity player = MakePlayer(registry, Vec2{5050.0f, 0.0f});  // Inside the radius.

    tutorial_system::Tick(MakeContext(world, intents, content, 7));

    REQUIRE(registry.all_of<SystemWarpRequest>(player));
    CHECK_FALSE(registry.get<SystemWarpRequest>(player).targetSystemId.empty());
}

TEST_CASE("TutorialSystem does not fire the cataclysm outside the trigger radius",
          "[tutorial][cataclysm]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity anomaly = registry.create();
    registry.emplace<WorldTransform>(anomaly, Vec2{5000.0f, 0.0f}, 0.0f);
    registry.emplace<AnomalyField>(anomaly, 400.0f);
    const entt::entity player = MakePlayer(registry, Vec2{0.0f, 0.0f});  // Far outside.

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<SystemWarpRequest>(player));
}

TEST_CASE("TutorialSystem's cataclysm does not re-fire on a lingering player",
          "[tutorial][cataclysm]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity anomaly = registry.create();
    registry.emplace<WorldTransform>(anomaly, Vec2{5000.0f, 0.0f}, 0.0f);
    registry.emplace<AnomalyField>(anomaly, 400.0f);
    const entt::entity player = MakePlayer(registry, Vec2{5050.0f, 0.0f});

    tutorial_system::Tick(MakeContext(world, intents, content, 1));
    const std::string firstTarget = registry.get<SystemWarpRequest>(player).targetSystemId;
    tutorial_system::Tick(MakeContext(world, intents, content, 2));

    CHECK(registry.get<SystemWarpRequest>(player).targetSystemId == firstTarget);
}

TEST_CASE("TutorialSystem's cataclysm fires regardless of the offer's outcome",
          "[tutorial][cataclysm]") {
    SystemWorld world("prologue");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity anomaly = registry.create();
    registry.emplace<WorldTransform>(anomaly, Vec2{5000.0f, 0.0f}, 0.0f);
    registry.emplace<AnomalyField>(anomaly, 400.0f);
    const entt::entity player = MakePlayer(registry, Vec2{5050.0f, 0.0f});
    registry.emplace<TutorialDeclined>(player);

    tutorial_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.all_of<SystemWarpRequest>(player));
}
