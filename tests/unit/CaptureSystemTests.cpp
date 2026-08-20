#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/CaptureSystem.h"
#include "shared/components/Capture.h"
#include "shared/components/Commander.h"
#include "shared/components/Docking.h"
#include "shared/components/Identity.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

using sr::BoardingHold;
using sr::Commander;
using sr::CommanderOrders;
using sr::Docked;
using sr::FactionId;
using sr::FactionRef;
using sr::KnowledgeNetworkId;
using sr::Rig;
using sr::Targetable;
using sr::Uncrewed;
using sr::Vec2;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace capture_system = sr::space::capture_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content, float dt) {
    return SystemContext{world, intents, content, dt, 0};
}

entt::entity MakeRig(entt::registry& registry, const Vec2& position, const char* faction) {
    const entt::entity root = registry.create();
    registry.emplace<WorldTransform>(root, position, 0.0f);
    registry.emplace<FactionRef>(root, FactionId(faction));
    registry.emplace<Targetable>(root);
    return root;
}

// A boarding hold that started last tick never completes in the same tick it is created
// (CaptureSystem.cpp), so every "capture completes" fixture needs two ticks: one to start the
// hold, one with a dt long enough to clear kBoardingHoldSeconds.
void StartThenCompleteHold(SystemWorld& world, const sr::core::IntentQueue& intents,
                           const sr::core::ContentLibrary& content) {
    capture_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f));
    capture_system::Tick(MakeContext(world, intents, content, 30.0f));
}

}  // namespace

TEST_CASE("CaptureSystem never captures a crewed hull, however long the hold", "[capture]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity target = MakeRig(registry, Vec2{0.0f, 0.0f}, "reavers");
    // Deliberately no Uncrewed tag: this hull still has living crew.
    const entt::entity boarder = MakeRig(registry, Vec2{10.0f, 0.0f}, "aegis");

    StartThenCompleteHold(world, intents, content);

    CHECK(registry.get<FactionRef>(target).id == FactionId("reavers"));
    CHECK_FALSE(registry.all_of<BoardingHold>(boarder));
}

TEST_CASE(
    "CaptureSystem flips FactionRef once a hold on an Uncrewed hostile hull completes, "
    "an AI boarder capturing the player's hull the identical way",
    "[capture]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    // "The player's hull" here means only that its FactionRef is the player's faction -- nothing
    // in CaptureSystem branches on PlayerControlled/PlayerLocation, so an AI boarder (faction
    // "reavers") takes it through the exact same path a player boarder would.
    const entt::entity target = MakeRig(registry, Vec2{0.0f, 0.0f}, "player");
    registry.emplace<Uncrewed>(target);
    const entt::entity boarder = MakeRig(registry, Vec2{10.0f, 0.0f}, "reavers");

    StartThenCompleteHold(world, intents, content);

    CHECK(registry.get<FactionRef>(target).id == FactionId("reavers"));
    CHECK_FALSE(registry.all_of<BoardingHold>(boarder));
}

TEST_CASE("CaptureSystem transfers a captured hull's crew and Commander network wholesale",
          "[capture]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity target = MakeRig(registry, Vec2{0.0f, 0.0f}, "reavers");
    registry.emplace<Uncrewed>(target);
    // A sub-commander riding the captured hull (features.md 2.5), on its Bridge/cockpit hardpoint
    // rather than the root (architecture.md 12.16 item 22, Commander.h): the network id is
    // anchored to this entity and must survive untouched, but `faction` must follow the hull.
    const KnowledgeNetworkId network("reaver-cmd-7");
    const entt::entity bridge = registry.create();
    registry.emplace<Commander>(bridge,
                                Commander{network, CommanderOrders::Defend, FactionId("reavers")});
    // A second, still-living hardpoint aboard -- nothing about capture should touch it.
    Rig rig;
    const entt::entity survivingHardpoint = registry.create();
    rig.children.push_back(bridge);
    rig.children.push_back(survivingHardpoint);
    registry.emplace<Rig>(target, rig);

    MakeRig(registry, Vec2{10.0f, 0.0f}, "aegis");

    StartThenCompleteHold(world, intents, content);

    CHECK(registry.get<FactionRef>(target).id == FactionId("aegis"));
    REQUIRE(registry.all_of<Commander>(bridge));
    CHECK(registry.get<Commander>(bridge).faction == FactionId("aegis"));
    CHECK(registry.get<Commander>(bridge).network == network);
    REQUIRE(registry.all_of<Rig>(target));
    CHECK(registry.get<Rig>(target).children.size() == 2);
    CHECK(registry.get<Rig>(target).children[1] == survivingHardpoint);
}

TEST_CASE("CaptureSystem does not board a target out of boarding range", "[capture]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity target = MakeRig(registry, Vec2{0.0f, 0.0f}, "reavers");
    registry.emplace<Uncrewed>(target);
    const entt::entity boarder = MakeRig(registry, Vec2{9000.0f, 0.0f}, "aegis");

    StartThenCompleteHold(world, intents, content);

    CHECK(registry.get<FactionRef>(target).id == FactionId("reavers"));
    CHECK_FALSE(registry.all_of<BoardingHold>(boarder));
}

TEST_CASE("CaptureSystem resets an interrupted hold instead of pausing it", "[capture]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity target = MakeRig(registry, Vec2{0.0f, 0.0f}, "reavers");
    registry.emplace<Uncrewed>(target);
    const entt::entity boarder = MakeRig(registry, Vec2{10.0f, 0.0f}, "aegis");

    // Starts a hold and lets it accrue partway toward completion.
    capture_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f));
    capture_system::Tick(MakeContext(world, intents, content, 5.0f));
    REQUIRE(registry.all_of<BoardingHold>(boarder));
    CHECK(registry.get<BoardingHold>(boarder).heldSeconds == Catch::Approx(5.0f));

    // The boarder pulls out of range: the hold is dropped, not merely paused.
    registry.get<WorldTransform>(boarder).position = Vec2{9000.0f, 0.0f};
    capture_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f));
    CHECK_FALSE(registry.all_of<BoardingHold>(boarder));

    // Coming back starts over at zero rather than resuming the earlier progress.
    registry.get<WorldTransform>(boarder).position = Vec2{10.0f, 0.0f};
    capture_system::Tick(MakeContext(world, intents, content, 1.0f / 60.0f));
    REQUIRE(registry.all_of<BoardingHold>(boarder));
    CHECK(registry.get<BoardingHold>(boarder).heldSeconds == Catch::Approx(0.0f));
    CHECK(registry.get<FactionRef>(target).id == FactionId("reavers"));
}

TEST_CASE("CaptureSystem never boards from a Docked or Uncrewed rig", "[capture]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity target = MakeRig(registry, Vec2{0.0f, 0.0f}, "reavers");
    registry.emplace<Uncrewed>(target);

    const entt::entity dockedBoarder = MakeRig(registry, Vec2{10.0f, 0.0f}, "aegis");
    registry.emplace<Docked>(dockedBoarder);

    const entt::entity uncrewedBoarder = MakeRig(registry, Vec2{-10.0f, 0.0f}, "aegis");
    registry.emplace<Uncrewed>(uncrewedBoarder);

    StartThenCompleteHold(world, intents, content);

    CHECK(registry.get<FactionRef>(target).id == FactionId("reavers"));
    CHECK_FALSE(registry.all_of<BoardingHold>(dockedBoarder));
    CHECK_FALSE(registry.all_of<BoardingHold>(uncrewedBoarder));
}
