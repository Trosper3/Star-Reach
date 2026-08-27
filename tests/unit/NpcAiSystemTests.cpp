#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/NpcAiSystem.h"
#include "shared/components/Ai.h"
#include "shared/components/Combat.h"
#include "shared/components/Docking.h"
#include "shared/components/Health.h"
#include "shared/components/Identity.h"
#include "shared/components/Orbit.h"
#include "shared/components/Party.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/StationServices.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"
#include "shared/math/Angle.h"

using Catch::Approx;
using sr::AiBehavior;
using sr::AiState;
using sr::Docked;
using sr::DockPrompt;
using sr::DockRequest;
using sr::FireIntent;
using sr::GravityWell;
using sr::Health;
using sr::PartyMember;
using sr::PlayerLocation;
using sr::RepairBilling;
using sr::RepairOrder;
using sr::Rig;
using sr::Target;
using sr::ThrustInput;
using sr::Uncrewed;
using sr::Vec2;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace npc_ai_system = sr::space::npc_ai_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

// An AI-driven rig at the origin, facing +x, with no target yet. Returns its entity.
entt::entity MakeSeeker(entt::registry& registry) {
    const entt::entity seeker = registry.create();
    registry.emplace<WorldTransform>(seeker, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<ThrustInput>(seeker);
    registry.emplace<Target>(seeker);
    return seeker;
}

entt::entity MakeTargetRig(entt::registry& registry, const Vec2& position) {
    const entt::entity rig = registry.create();
    registry.emplace<WorldTransform>(rig, position, 0.0f);
    return rig;
}

}  // namespace

TEST_CASE("NpcAiSystem does nothing for a seeker with no acquired target", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.0f));
    CHECK_FALSE(registry.all_of<FireIntent>(seeker));
    CHECK(registry.get<AiBehavior>(seeker).state == AiState::Patrol);
}

TEST_CASE("NpcAiSystem thrusts forward and requests fire once aimed at a far target", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    const entt::entity enemy = MakeTargetRig(registry, Vec2{1000.0f, 0.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(1.0f));
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.0f));
    CHECK(registry.all_of<FireIntent>(seeker));
    // Beyond kEngageRangeUnits: still closing, not yet holding at range.
    CHECK(registry.get<AiBehavior>(seeker).state == AiState::Chase);
}

TEST_CASE("NpcAiSystem turns toward a target that is not dead ahead", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    // Directly "above" the seeker (+y): a 90-degree heading error.
    const entt::entity enemy = MakeTargetRig(registry, Vec2{0.0f, 1000.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    // Half the turn command at a quarter-turn heading error (proportional, clamped at +-1).
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.5f));
    // Broadside to the target: do not burn the main engine into a worse angle.
    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
}

TEST_CASE("NpcAiSystem stops closing once within engagement range but keeps requesting fire",
          "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    const entt::entity enemy = MakeTargetRig(registry, Vec2{100.0f, 0.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
    CHECK(registry.all_of<FireIntent>(seeker));
    // Within kEngageRangeUnits: holding at range rather than still closing.
    CHECK(registry.get<AiBehavior>(seeker).state == AiState::Attack);
}

TEST_CASE("NpcAiSystem clears stale thrust and fire intent once a target is lost", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    registry.get<ThrustInput>(seeker) = ThrustInput{1.0f, 0.0f, 1.0f};
    registry.emplace<FireIntent>(seeker);
    registry.emplace<AiBehavior>(seeker, AiState::Chase);
    // Target.rig stays entt::null (never acquired / lost by TargetingSystem this tick).

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.0f));
    CHECK_FALSE(registry.all_of<FireIntent>(seeker));
    // Returns to Patrol rather than freezing in whatever state it lost the target in.
    CHECK(registry.get<AiBehavior>(seeker).state == AiState::Patrol);
}

TEST_CASE("NpcAiSystem never drives a Docked rig, even with a live target", "[npc_ai]") {
    // Regression test for architecture.md 13.3 finding H: a docked NPC must not have its throttle
    // rewritten or FireIntent re-emplaced -- DockingSystem zeroes both once, on dock, and an
    // unexcluded NpcAiSystem would undo that every tick after.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    registry.emplace<Docked>(seeker);
    const entt::entity enemy = MakeTargetRig(registry, Vec2{1000.0f, 0.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.0f));
    CHECK_FALSE(registry.all_of<FireIntent>(seeker));
}

TEST_CASE("NpcAiSystem flees a GravityWell instead of pursuing a target across it", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    // Well centred at the origin; seeker sits well inside range + the avoidance margin.
    const entt::entity sun = registry.create();
    registry.emplace<WorldTransform>(sun, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<GravityWell>(sun, 2200.0f, 260.0f);

    // MakeSeeker's default rotation (0, i.e. +x) already faces directly away from a well
    // centred at the origin -- no turn needed, just a straight burn clear.
    const entt::entity seeker = MakeSeeker(registry);
    registry.get<WorldTransform>(seeker).position = Vec2{500.0f, 0.0f};
    // Target on the far side of the well: straight-line pursuit would cut through it.
    const entt::entity enemy = MakeTargetRig(registry, Vec2{-500.0f, 0.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.0f));
    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(1.0f));
    CHECK_FALSE(registry.all_of<FireIntent>(seeker));
}

TEST_CASE("NpcAiSystem pursues normally once outside every GravityWell's avoidance range",
          "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity sun = registry.create();
    registry.emplace<WorldTransform>(sun, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<GravityWell>(sun, 2200.0f, 260.0f);

    const entt::entity seeker = MakeSeeker(registry);
    registry.get<WorldTransform>(seeker).position = Vec2{3000.0f, 0.0f};  // Outside range + margin.
    const entt::entity enemy = MakeTargetRig(registry, Vec2{4000.0f, 0.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(1.0f));
    CHECK(registry.all_of<FireIntent>(seeker));
}

TEST_CASE("NpcAiSystem never drives an Uncrewed rig, even with a live target", "[npc_ai]") {
    // features.md 3.2's uncrewed hull: no living control-shell crew means the rig stops flying
    // and firing, without being destroyed -- the same shape as the Docked exclusion above.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    registry.emplace<Uncrewed>(seeker);
    const entt::entity enemy = MakeTargetRig(registry, Vec2{1000.0f, 0.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.0f));
    CHECK_FALSE(registry.all_of<FireIntent>(seeker));
}

TEST_CASE("NpcAiSystem never drives the PlayerLocation rig", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity player = MakeSeeker(registry);
    registry.emplace<PlayerLocation>(player, PlayerLocation{player});
    const entt::entity enemy = MakeTargetRig(registry, Vec2{1000.0f, 0.0f});
    registry.get<Target>(player).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(player).forward == Approx(0.0f));
    CHECK_FALSE(registry.all_of<FireIntent>(player));
}

TEST_CASE("NpcAiSystem flees and stops firing once structural integrity drops below threshold",
          "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    // 20% aggregate integrity: below kFleeIntegrityFraction (0.5), well above
    // rig_attachment::kStructuralFailureThreshold (0.30) so DamageSystem would not already have
    // destroyed the rig outright.
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 20.0f, 100.0f);
    registry.emplace<Rig>(seeker, std::vector<entt::entity>{hardpoint});

    // Enemy due south: away-from-threat is due north, a clean quarter turn with no +-pi
    // wraparound ambiguity (the same reason the "not dead ahead" test above picks +y).
    const entt::entity enemy = MakeTargetRig(registry, Vec2{0.0f, -1000.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<AiBehavior>(seeker).state == AiState::Flee);
    CHECK_FALSE(registry.all_of<FireIntent>(seeker));
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.5f));
    // Not yet aligned with the flee heading: still turning, not yet burning.
    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
    CHECK_FALSE(registry.all_of<DockRequest>(seeker));  // No DockPrompt this tick.
}

TEST_CASE("NpcAiSystem requests the DockPrompt's bay while fleeing", "[npc_ai]") {
    // architecture.md 12.30.4: Flee is the disengagement; this gives it a destination.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 20.0f, 100.0f);
    registry.emplace<Rig>(seeker, std::vector<entt::entity>{hardpoint});

    const entt::entity enemy = MakeTargetRig(registry, Vec2{0.0f, -1000.0f});
    registry.get<Target>(seeker).rig = enemy;

    const entt::entity bay = registry.create();
    registry.emplace<DockPrompt>(seeker, bay);

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<AiBehavior>(seeker).state == AiState::Flee);
    REQUIRE(registry.all_of<DockRequest>(seeker));
    CHECK(registry.get<DockRequest>(seeker).bay == bay);
}

TEST_CASE("NpcAiSystem orders repair for a docked, damaged, non-player rig", "[npc_ai]") {
    // architecture.md 12.30.4: NPC repair's other half -- the AI producer RepairOrder had none of
    // before this.
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = registry.create();
    const entt::entity npc = registry.create();
    registry.emplace<Docked>(npc, station, entt::null);
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);
    registry.emplace<Rig>(npc, std::vector<entt::entity>{hardpoint});

    npc_ai_system::Tick(MakeContext(world, intents, content));

    REQUIRE(registry.all_of<RepairOrder>(npc));
    const RepairOrder& order = registry.get<RepairOrder>(npc);
    CHECK(order.subject == npc);
    CHECK((order.hardpoint == entt::null));
    CHECK(order.targetFraction == Approx(1.0f));
}

TEST_CASE("NpcAiSystem does not order repair for a docked rig already at full integrity",
          "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = registry.create();
    const entt::entity npc = registry.create();
    registry.emplace<Docked>(npc, station, entt::null);
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 100.0f, 100.0f);
    registry.emplace<Rig>(npc, std::vector<entt::entity>{hardpoint});

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<RepairOrder>(npc));
}

TEST_CASE("NpcAiSystem does not overwrite an in-progress RepairOrder", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = registry.create();
    const entt::entity npc = registry.create();
    registry.emplace<Docked>(npc, station, entt::null);
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);
    registry.emplace<Rig>(npc, std::vector<entt::entity>{hardpoint});
    registry.emplace<RepairOrder>(npc, RepairOrder{npc, entt::null, 1.0f});
    // The fractional credit carry lives on RepairBilling, not RepairOrder, since issue #268 --
    // this rig already has one in progress, and re-emplacing the order must not disturb it.
    registry.emplace<RepairBilling>(npc, RepairBilling{0.75f});

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.all_of<RepairOrder>(npc));
    CHECK(registry.get<RepairBilling>(npc).creditRemainder == Approx(0.75f));
}

TEST_CASE("NpcAiSystem does not order repair for the player's own docked rig", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity station = registry.create();
    const entt::entity player = registry.create();
    registry.emplace<Docked>(player, station, entt::null);
    registry.emplace<PlayerLocation>(player, PlayerLocation{player});
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 50.0f, 100.0f);
    registry.emplace<Rig>(player, std::vector<entt::entity>{hardpoint});

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK_FALSE(registry.all_of<RepairOrder>(player));
}

TEST_CASE("NpcAiSystem does not flee a healthy rig even with a live target", "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    const entt::entity hardpoint = registry.create();
    registry.emplace<Health>(hardpoint, 90.0f, 100.0f);
    registry.emplace<Rig>(seeker, std::vector<entt::entity>{hardpoint});
    const entt::entity enemy = MakeTargetRig(registry, Vec2{1000.0f, 0.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<AiBehavior>(seeker).state == AiState::Chase);
    CHECK(registry.all_of<FireIntent>(seeker));
}

TEST_CASE(
    "NpcAiSystem gives up a chase beyond its own reach, clearing Target and returning to Patrol",
    "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    // MakeSeeker carries no SensorRange, so Reach() falls back to kFallbackReachUnits (2000) --
    // beyond it a chase is not worth continuing (TargetingSystem's own IsValidTarget never drops
    // a Target for distance alone, so without this an NPC would chase indefinitely).
    const entt::entity enemy = MakeTargetRig(registry, Vec2{5000.0f, 0.0f});
    registry.get<Target>(seeker).rig = enemy;

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<AiBehavior>(seeker).state == AiState::Patrol);
    CHECK((registry.get<Target>(seeker).rig == entt::null));
    CHECK_FALSE(registry.all_of<FireIntent>(seeker));
    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
}

TEST_CASE(
    "NpcAiSystem pursues an assigned escort target without acquiring PartyMember, holding "
    "station once in range",
    "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    // Due north and beyond kEscortHoldRangeUnits: still closing.
    const entt::entity friendly = MakeTargetRig(registry, Vec2{0.0f, 1000.0f});
    registry.emplace<AiBehavior>(seeker, AiState::Patrol, friendly);

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<AiBehavior>(seeker).state == AiState::Escort);
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.5f));
    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
    CHECK_FALSE(registry.all_of<FireIntent>(seeker));
    CHECK_FALSE(registry.all_of<PartyMember>(seeker));
}

TEST_CASE("NpcAiSystem holds station once within escort range instead of continuing to close",
          "[npc_ai]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity seeker = MakeSeeker(registry);
    const entt::entity friendly = MakeTargetRig(registry, Vec2{100.0f, 0.0f});  // inside hold range
    registry.emplace<AiBehavior>(seeker, AiState::Patrol, friendly);

    npc_ai_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<AiBehavior>(seeker).state == AiState::Escort);
    CHECK(registry.get<ThrustInput>(seeker).turn == Approx(0.0f));
    CHECK(registry.get<ThrustInput>(seeker).forward == Approx(0.0f));
}
