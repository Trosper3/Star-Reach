#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/events/IntentQueue.h"
#include "core/registries/ContentLibrary.h"
#include "modes/space/data/SystemWorld.h"
#include "modes/space/systems/PartySystem.h"
#include "shared/components/Health.h"
#include "shared/components/Party.h"
#include "shared/components/Physics.h"
#include "shared/components/Rig.h"
#include "shared/components/Targeting.h"
#include "shared/components/Transform.h"

using Catch::Approx;
using sr::PartyLeader;
using sr::PartyMember;
using sr::PendingDamage;
using sr::Rig;
using sr::Target;
using sr::ThrustInput;
using sr::Vec2;
using sr::WorldTransform;
using sr::space::SystemContext;
using sr::space::SystemWorld;
namespace party_system = sr::space::party_system;

namespace {

SystemContext MakeContext(SystemWorld& world, const sr::core::IntentQueue& intents,
                          const sr::core::ContentLibrary& content) {
    return SystemContext{world, intents, content, 1.0f / 60.0f, 0};
}

entt::entity MakeMember(entt::registry& registry, entt::entity leader, const Vec2& position,
                        const Vec2& formationOffset) {
    const entt::entity member = registry.create();
    registry.emplace<WorldTransform>(member, position, 0.0f);
    registry.emplace<ThrustInput>(member);
    registry.emplace<Target>(member);
    registry.emplace<PartyMember>(member, leader, formationOffset);
    return member;
}

entt::entity MakeHardpoint(entt::registry& registry, Rig& rig) {
    const entt::entity hardpoint = registry.create();
    rig.children.push_back(hardpoint);
    return hardpoint;
}

}  // namespace

TEST_CASE("PartySystem steers a member toward its formation slot", "[party]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity leader = registry.create();
    registry.emplace<WorldTransform>(leader, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<PartyLeader>(leader);

    // Formation slot is "above" the leader (+y); member sits far below it (-y), a 90-degree
    // heading error, well outside the arrive radius.
    const entt::entity member =
        MakeMember(registry, leader, Vec2{0.0f, -1000.0f}, Vec2{0.0f, 100.0f});

    party_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(member).turn == Approx(0.5f));
    CHECK(registry.get<ThrustInput>(member).forward == Approx(0.0f));
}

TEST_CASE("PartySystem thrusts a member forward once aimed at a far formation slot", "[party]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity leader = registry.create();
    registry.emplace<WorldTransform>(leader, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<PartyLeader>(leader);

    // Slot is at (100, 0); member already faces +x (rotation 0) from far behind it.
    const entt::entity member =
        MakeMember(registry, leader, Vec2{-1000.0f, 0.0f}, Vec2{100.0f, 0.0f});

    party_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(member).forward == Approx(1.0f));
}

TEST_CASE("PartySystem stops a member that has arrived at its formation slot", "[party]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity leader = registry.create();
    registry.emplace<WorldTransform>(leader, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<PartyLeader>(leader);

    // Member already sitting on its formation slot -- well inside the arrive radius.
    const entt::entity member = MakeMember(registry, leader, Vec2{50.0f, 0.0f}, Vec2{50.0f, 0.0f});
    registry.get<ThrustInput>(member) = ThrustInput{1.0f, 0.0f, 1.0f};

    party_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(member).forward == Approx(0.0f));
    CHECK(registry.get<ThrustInput>(member).turn == Approx(0.0f));
}

TEST_CASE("PartySystem leaves an engaging member's ThrustInput alone", "[party]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity leader = registry.create();
    registry.emplace<WorldTransform>(leader, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<PartyLeader>(leader);

    const entt::entity member =
        MakeMember(registry, leader, Vec2{0.0f, -1000.0f}, Vec2{0.0f, 100.0f});
    // NpcAiSystem already drove this member toward its combat target this tick.
    registry.get<ThrustInput>(member) = ThrustInput{0.75f, 0.0f, -0.25f};
    const entt::entity hostile = registry.create();
    registry.get<Target>(member).rig = hostile;

    party_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<ThrustInput>(member).forward == Approx(0.75f));
    CHECK(registry.get<ThrustInput>(member).turn == Approx(-0.25f));
}

TEST_CASE("PartySystem alerts idle members when the leader takes damage", "[party]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity attacker = registry.create();

    const entt::entity leader = registry.create();
    registry.emplace<WorldTransform>(leader, Vec2{0.0f, 0.0f}, 0.0f);
    Rig leaderRig;
    const entt::entity leaderHull = MakeHardpoint(registry, leaderRig);
    registry.emplace<Rig>(leader, leaderRig);
    registry.emplace<PendingDamage>(leaderHull, 10.0f, sr::DamageType::Kinetic, attacker);
    registry.emplace<Target>(leader);  // Leader itself has no target yet either.
    PartyLeader& party = registry.emplace<PartyLeader>(leader);

    const entt::entity member =
        MakeMember(registry, leader, Vec2{0.0f, -50.0f}, Vec2{0.0f, 100.0f});
    party.members.push_back(member);

    party_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Target>(leader).rig == attacker);
    CHECK(registry.get<Target>(member).rig == attacker);
}

TEST_CASE("PartySystem does not override a member already fighting something else", "[party]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity attacker = registry.create();
    const entt::entity priorTarget = registry.create();

    const entt::entity leader = registry.create();
    registry.emplace<WorldTransform>(leader, Vec2{0.0f, 0.0f}, 0.0f);
    Rig leaderRig;
    const entt::entity leaderHull = MakeHardpoint(registry, leaderRig);
    registry.emplace<Rig>(leader, leaderRig);
    registry.emplace<PendingDamage>(leaderHull, 10.0f, sr::DamageType::Kinetic, attacker);
    registry.emplace<Target>(leader);
    PartyLeader& party = registry.emplace<PartyLeader>(leader);

    const entt::entity member =
        MakeMember(registry, leader, Vec2{0.0f, -50.0f}, Vec2{0.0f, 100.0f});
    registry.get<Target>(member).rig = priorTarget;
    party.members.push_back(member);

    party_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Target>(member).rig == priorTarget);
}

TEST_CASE("PartySystem propagates an attack on a member back to the leader", "[party]") {
    SystemWorld world("sol");
    entt::registry& registry = world.Registry();
    sr::core::IntentQueue intents;
    sr::core::ContentLibrary content;

    const entt::entity attacker = registry.create();

    const entt::entity leader = registry.create();
    registry.emplace<WorldTransform>(leader, Vec2{0.0f, 0.0f}, 0.0f);
    registry.emplace<Rig>(leader);
    registry.emplace<Target>(leader);
    PartyLeader& party = registry.emplace<PartyLeader>(leader);

    const entt::entity member =
        MakeMember(registry, leader, Vec2{0.0f, -50.0f}, Vec2{0.0f, 100.0f});
    Rig memberRig;
    const entt::entity memberHull = MakeHardpoint(registry, memberRig);
    registry.emplace<Rig>(member, memberRig);
    registry.emplace<PendingDamage>(memberHull, 10.0f, sr::DamageType::Kinetic, attacker);
    party.members.push_back(member);

    party_system::Tick(MakeContext(world, intents, content));

    CHECK(registry.get<Target>(leader).rig == attacker);
}
